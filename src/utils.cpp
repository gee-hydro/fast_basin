#include "utils.h"
#include <iostream>
#include <string>

#include "DEM.h"
#include "FlowDirection.h"
#include "gdal_priv.h"
#include "ogr_geometry.h"

bool  CreateGeoTIFF(char* path,int height, int width,void* pData, GDALDataType type, double* geoTransformArray6Eles,
					double* min, double* max, double* mean, double* stdDev, double nodatavalue)
{
    GDALDataset *poDataset;   
    GDALAllRegister();
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8","NO");
 
	GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName("GTiff");
    char **papszOptions = NULL;
	papszOptions = CSLSetNameValue(papszOptions, "COMPRESS", "LZW");
	papszOptions = CSLSetNameValue(papszOptions, "TILED", "YES");
	papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "YES"); //配置图像信息
	poDataset = poDriver->Create(path, width, height, 1, type,
								 papszOptions);
	if(poDataset == NULL)
	{
		const char* errorMsg = CPLGetLastErrorMsg();
	}
	if (geoTransformArray6Eles != NULL) {
		poDataset->SetGeoTransform(geoTransformArray6Eles);
		// 设置大地坐标系统
		// https:  //blog.csdn.net/L_J_Kin/article/details/103403122
		OGRSpatialReference oSRS;
		char* pszSRS_WKT = NULL;
		oSRS.SetWellKnownGeogCS("WGS84");
		oSRS.exportToWkt(&pszSRS_WKT);
		poDataset->SetProjection(pszSRS_WKT);
		CPLFree(pszSRS_WKT);  //使用完后释放
	}

    GDALRasterBand* poBand;
    poBand = poDataset->GetRasterBand(1);

    poBand->SetNoDataValue(nodatavalue);

    if (min != NULL && max != NULL && mean != NULL && stdDev != NULL) {
        poBand->SetStatistics(*min, *max, *mean, *stdDev);
    }
    CPLErr error = poBand->RasterIO(GF_Write, 0, 0, width, height,
                                    pData, width, height, type, 0, 0);

    GDALClose((GDALDatasetH)poDataset);

    return true;
}

//read GeoTIFF file into a DEM, supporting the reading of data types of int16£¬int32£¬float
bool readTIFF(const char* path, DEM& dem, double* geoTransformArray6Eles)
{
	std::cout<<"Reading input DEM file..."<<std::endl;
	GDALDataset *poDataset;   
    GDALAllRegister();
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8","NO");
	poDataset = (GDALDataset* )GDALOpen(path, GA_ReadOnly);
	const char* errorMsg = CPLGetLastErrorMsg();
	if (poDataset == NULL)
	{
		printf("Failed to open the GeoTIFF file\n");
		return false;
	}

	GDALRasterBand* poBand;
	poBand = poDataset->GetRasterBand(1);
	GDALDataType dataType = poBand->GetRasterDataType();

	bigint width = poBand->GetXSize();
	bigint height = poBand->GetYSize();
	double originNoDataValue = poBand->GetNoDataValue();
	if (dataType == GDALDataType::GDT_Int16)
	{
		short* fromDem = NULL;
		try
		{
			fromDem = new short[width * height];
		}
		catch (std::bad_alloc& e)
		{
			fprintf(stderr, "Failed to allocate memory for reading input file\n");
			GDALClose((GDALDatasetH)poDataset);
			return false;
		}
		
		CPLErr status = poBand->RasterIO(GF_Read, 0, 0, width, height, 
			(void *)fromDem, width, height, GDALDataType::GDT_Int16, 0, 0);

		dem.SetWidth(width);
		dem.SetHeight(height);
		if(!dem.Allocate())
		{
			fprintf(stderr, "Failed to allocate memory for reading input file\n");
			GDALClose((GDALDatasetH)poDataset);
			delete[] fromDem;
			return false;
		}

		if(!int16ToFloat32(fromDem, dem, width, height))
		{
			fprintf(stderr, "Failed to read input file\n");
			GDALClose((GDALDatasetH)poDataset);
			delete[] fromDem;
			return false;
		}

	}
	else if(dataType == GDALDataType::GDT_Int32)
	{
		int* fromDem = NULL;
		try
		{
			fromDem = new int[width * height];
		}
		catch (std::bad_alloc& e)
		{
			fprintf(stderr, "Failed to allocate memory for reading input file\n");
			GDALClose((GDALDatasetH)poDataset);
			return false;
		}

		CPLErr status = poBand->RasterIO(GF_Read, 0, 0, width, height, 
			(void *)fromDem, width, height, GDALDataType::GDT_Int32, 0, 0);

		dem.SetWidth(width);
		dem.SetHeight(height);
		if(!dem.Allocate())
		{
			fprintf(stderr, "Failed to allocate memory for reading input file\n");
			GDALClose((GDALDatasetH)poDataset);
			delete[] fromDem;
			return false;
		}

		if(!int32ToFloat32(fromDem, dem, width, height))
		{
			fprintf(stderr, "Failed to read input file\n");
			GDALClose((GDALDatasetH)poDataset);
			delete[] fromDem;
			return false;
		}
	}
	else if(dataType == GDALDataType::GDT_Float32)
	{
		dem.SetWidth(width);
		dem.SetHeight(height);
		if(!dem.Allocate())
		{
			fprintf(stderr, "Failed to allocate memory for reading input file\n");
			GDALClose((GDALDatasetH)poDataset);
			return false;
		}

		CPLErr status = poBand->RasterIO(GF_Read, 0, 0, width, height, 
			(void *)dem.getDEMdata(), width, height, GDALDataType::GDT_Float32, 0, 0);

	}
	else
	{
		fprintf(stderr, "Unsupported DEM data type. Curretly only short, int and float datatypes are supported£¡\n");
		GDALClose((GDALDatasetH)poDataset);
		return false;
	}
	if (geoTransformArray6Eles == NULL)
	{
		return false;
	}
	memset(geoTransformArray6Eles, 0, 6);
	poDataset->GetGeoTransform(geoTransformArray6Eles);

	changeNoDataValue(dem, width, height, originNoDataValue, -9999.0f);
	GDALClose((GDALDatasetH)poDataset);
	std::cout<<"Finish reading GeoTIFF file!\n";
	return true;
}
// read GeoTIFF into a flow direction matrix
bool readTIFF(const char* path, GDALDataType type, FlowDirection& dem, double* geoTransformArray6Eles)
{
	std::cout<<"Reading input DEM file..."<<std::endl;
	GDALDataset *poDataset;  
	GDALAllRegister();
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8","NO");
	poDataset = (GDALDataset* )GDALOpen(path, GA_ReadOnly);
	const char* errorMsg = CPLGetLastErrorMsg();
	if (poDataset == NULL)
	{
		printf("Failed to open the GeoTIFF file\n");
		return false;
	}

	GDALRasterBand* poBand;
	poBand = poDataset->GetRasterBand(1);
	GDALDataType dataType = poBand->GetRasterDataType();
	if (dataType != type)
	{
		return false;
	}
	if (geoTransformArray6Eles == NULL)
	{
		return false;
	}

	memset(geoTransformArray6Eles, 0, 6);
	poDataset->GetGeoTransform(geoTransformArray6Eles);

	dem.SetWidth(poBand->GetXSize());
	dem.SetHeight(poBand->GetYSize());

	if (!dem.Allocate()) return false;

	CPLErr status = poBand->RasterIO(GF_Read, 0, 0, dem.Get_NX(), dem.Get_NY(), 
		(void *)dem.getData(), dem.Get_NX(), dem.Get_NY(), dataType, 0, 0);

	GDALClose((GDALDatasetH)poDataset);
	std::cout<<"Finish reading GeoTIFF file!\n";
	return true;
}

/*
*	8-neighbor
*	5  6  7
*	4     0
*	3  2  1
*/
int Get_rowTo(int dir, int row)
{
	static int	ix[8]	= { 0, 1, 1, 1, 0,-1,-1,-1 };
	return( row + ix[dir] );
}
int Get_colTo(int dir, int col)
{
	static int	iy[8]	= { 1, 1, 0,-1,-1,-1, 0, 1 };
	return( col + iy[dir] );
}

void setNoData(unsigned char* data, int length, unsigned char noDataValue)
{
	if (data == NULL || length == 0)
	{
		return;
	}

	for (int i = 0; i < length; i++)
	{
		data[i] = noDataValue;
	}
}
void setNoData(float* data, int length, float noDataValue)
{
	if (data == NULL || length == 0)
	{
		return;
	}
	for (int i = 0; i < length; i++)
	{
		data[i] = noDataValue;
	}
}

const unsigned char value[8] = {128, 64, 32, 16, 8, 4, 2, 1};

void setFlag(int index, unsigned char* flagArray)
{
	int indexOfByte = index / 8;

	int indexInByte = index % 8;

	flagArray[indexOfByte] |= value[indexInByte];
}

bool isProcessed(int index, const unsigned char* flagArray)
{
	int indexOfByte = index / 8;
	int indexInByte = index % 8;
	unsigned char buf = flagArray[indexOfByte];
	buf &= value[indexInByte];
	if (buf>0) return true;
	return false;
}


//Algorithm 1. Compute the NIDP matrix from FlowDir matrix.
void calNips(FlowDirection& dirGrid, FlowDirection& nipsGrid)
{
	unsigned int width = dirGrid.getWidth();
	unsigned int height = dirGrid.getHeight();

	memset(nipsGrid.getData(), 0, width * height);
	for(unsigned int row = 0; row < height; ++row)
	{
		for(unsigned int col = 0; col < width; ++col)
		{
			if(dirGrid.is_NoData(row, col)) continue;

			unsigned int nextRow = row;
			unsigned int nextCol = col;

			if(!dirGrid.nextCell(nextRow, nextCol, dirGrid.asByte(row, col)))
				continue;
			else
				nipsGrid.Set_Value(nextRow, nextCol, nipsGrid.asByte(nextRow, nextCol) + 1);

		}
	}
}
bool int16ToFloat32(const short* fromGrid, DEM& toDem, unsigned int width, unsigned int height)
{
	if(fromGrid == NULL) return false;

	float* pToDemData = toDem.getDEMdata();
	unsigned int sum = width * height;
	for(unsigned int i = 0; i < sum; ++i)
	{
		pToDemData[i] = (float)fromGrid[i];
	}
	return true;
}
bool int32ToFloat32(const int* fromGrid, DEM& toDem, unsigned int width, unsigned int height)
{
	if(fromGrid == NULL) return false;
	float* pToDemData = toDem.getDEMdata();
	unsigned int sum = width * height;
	for(unsigned int i = 0; i < sum; ++i)
	{
		pToDemData[i] = (float)fromGrid[i];
	}
	return true;
}
void changeNoDataValue(DEM& toDem, unsigned int width, unsigned int height, float originNoDataValue, float toNoDataValue)
{
	unsigned int sum = width * height;
	float* toDemData = toDem.getDEMdata();
	for(unsigned int i = 0; i < sum; ++i)
	{
		if(fabs(toDemData[i] - originNoDataValue) < 0.000001)
		{
			toDemData[i] = toNoDataValue;
		}
	}
}

//Compute statistics for a DEM matrix
void calculateStatistics(const DEM& dem, double* min, double* max, double* mean, double* stdDev) {
    int width = dem.Get_NX();
    int height = dem.Get_NY();

    int validElements = 0;
    double minValue, maxValue;
    double sum = 0.0;
    double sumSqurVal = 0.0;
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            if (!dem.is_NoData(row, col)) {
                double value = dem.asFloat(row, col);
                //Initialize minValue and maxValue using the first valid value
                if (validElements == 0) {
                    minValue = maxValue = value;
                }
                validElements++;
                if (minValue > value) {
                    minValue = value;
                }
                if (maxValue < value) {
                    maxValue = value;
                }

                sum += value;
                sumSqurVal += (value * value);
            }
        }
    }

    double meanValue = sum / validElements;
    double stdDevValue = sqrt((sumSqurVal / validElements) - (meanValue * meanValue));
    *min = minValue;
    *max = maxValue;
    *mean = meanValue;
    *stdDev = stdDevValue;
}

//Compute statistics for a FlowDirection matrix
void calculateStatistics(const FlowDirection& dem, double* min, double* max, double* mean, double* stdDev) {
    int width = dem.Get_NX();
    int height = dem.Get_NY();

    int validElements = 0;
    double minValue, maxValue;
    double sum = 0.0;
    double sumSqurVal = 0.0;
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            if (!dem.is_NoData(row, col)) {
                double value = dem.asByte(row, col);
                //Initialize minValue and maxValue using the first valid value
                if (validElements == 0) {
                    minValue = maxValue = value;
                }
                validElements++;
                if (minValue > value) {
                    minValue = value;
                }
                if (maxValue < value) {
                    maxValue = value;
                }

                sum += value;
                sumSqurVal += (value * value);
            }
        }
    }

    double meanValue = sum / validElements;
    double stdDevValue = sqrt((sumSqurVal / validElements) - (meanValue * meanValue));
    *min = minValue;
    *max = maxValue;
    *mean = meanValue;
    *stdDev = stdDevValue;
}
