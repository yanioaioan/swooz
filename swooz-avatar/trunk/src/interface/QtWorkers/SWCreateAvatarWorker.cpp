
/*******************************************************************************
**                                                                            **
**  SWoOz is a software platform written in C++ used for behavioral           **
**  experiments based on interactions between people and robots               **
**  or 3D avatars.                                                            **
**                                                                            **
**  This program is free software: you can redistribute it and/or modify      **
**  it under the terms of the GNU Lesser General Public License as published  **
**  by the Free Software Foundation, either version 3 of the License, or      **
**  (at your option) any later version.                                       **
**                                                                            **
**  This program is distributed in the hope that it will be useful,           **
**  but WITHOUT ANY WARRANTY; without even the implied warranty of            **
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             **
**  GNU Lesser General Public License for more details.                       **
**                                                                            **
**  You should have received a copy of the GNU Lesser General Public License  **
**  along with Foobar.  If not, see <http://www.gnu.org/licenses/>.           **
**                                                                            **
** *****************************************************************************
**          Authors: Guillaume Gibert, Florian Lance                          **
**  Website/Contact: http://swooz.free.fr/                                    **
**       Repository: https://github.com/GuillaumeGibert/swooz                 **
********************************************************************************/

/**
 * \file SWDetectWorker.cpp
 * \author LANCE Florian
 * \date 08/02/13
 * \brief definition of SWDetectWorker
 */


#include "interface/QtWorkers/SWCreateAvatarWorker.h"
#include "cloud/SWImageProcessing.h"

#include "opencvUtility.h"

#include "moc_SWCreateAvatarWorker.cpp"

#include <QTime>
#include <QApplication>
#include <QVector3D>
#include <QVector2D>
#include <QWidget>

#include "cloud/SWIOCloud.h"
#include "cloud/SWConvCloud.h"


#include <time.h>

using namespace cv;
using namespace swDevice;

SWCreateAvatarWorker::SWCreateAvatarWorker(swDevice::SWKinect_thread *pRGBDDeviceThread) :
    m_pRGBDDeviceThread(pRGBDDeviceThread)
{
    m_bSendStasmPoints = false;
    m_bInitKinect = false;
    m_pFaceMeshResult = NULL;
    m_pCloudToDisplay = NULL;
    m_pCurrentFaceRect = NULL;
    m_pCurrentNoseRect = NULL;
    m_pRadialProjectionToDisplay = NULL;
    m_pFaceTexture = NULL;

    m_i32CurrentCloudNumber = 0;
    m_i32NumberOfClouds = 10;

    m_i32NumberStasm = 0;
    m_i32MaxNumberStasm = 5;


    // add new objets in qt meta type
        qRegisterMetaType<swCloud::SWCloud*>("SWCloud");
        qRegisterMetaType<cv::Mat*>("Mat");
        qRegisterMetaType<std::vector<cv::Point2i> >("std::vector<cv::Point2i>");

    // init create avatar
        m_CAvatarPtr = SWCreateAvatarPtr(new SWCreateAvatar(true));

    // init kinect thread used by the display widget
    if(m_pRGBDDeviceThread->init(0) != -1)
    {
        try
        {
            m_pRGBDDeviceThread->startListening();
            m_bInitKinect = true;
        }
        catch(const swExcept::kinectInitError &e)
        {
            std::cerr << "-ERROR : " << e.what() << std::endl;
            emit noKinectSignal();
        }
    }
    else
    {
        emit noKinectSignal();
    }
}

SWCreateAvatarWorker::~SWCreateAvatarWorker()
{
    deleteAndNullify(m_pCloudToDisplay);
    deleteAndNullify(m_pFaceMeshResult);
    deleteAndNullify(m_pRadialProjectionToDisplay);
    deleteAndNullify(m_pFaceTexture);
}

void SWCreateAvatarWorker::doWork()
{
    bool l_bContinueLoop = false;
    bool l_bCamCloudInitialized = false;

    if(m_bInitKinect)
    {
        m_bDoWork = true;
        l_bContinueLoop = true;
    }

    m_i32CurrentCloudNumber = 0;

    while(l_bContinueLoop)
    {
//        clock_t l_timeTraining = clock();

        // check if must stop loop
            m_oLoopMutex.lockForRead();
                l_bContinueLoop = m_bDoWork;
            m_oLoopMutex.unlock();

        // wait (necessary to get the events)
            QTime l_oDieTime = QTime::currentTime().addMSecs(3);
            while( QTime::currentTime() < l_oDieTime)
            {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 3);
            }



        // retrieve kinect data
            cv::Mat l_oBGR, l_oCloud;
            l_oBGR   = m_pRGBDDeviceThread->bgrImage().clone();;
            l_oCloud = m_pRGBDDeviceThread->cloudMap().clone();;

        // check size mat
           if(l_oBGR.rows != l_oCloud.rows)
           {
               cv::resize(l_oBGR, l_oBGR, cv::Size(l_oCloud.cols,l_oCloud.rows));
           }

        // main
        if(l_bContinueLoop)
        {
            if(m_i32CurrentCloudNumber < m_i32NumberOfClouds)
            {
                if(m_i32CurrentCloudNumber == 0)
                {
                    m_CAvatarPtr->resetData();
                }

                swCloud::SWCloud *l_pCloudToDisplay = NULL;

                bool l_bAddCloudSuccess = m_CAvatarPtr->addCloudToAvatar(l_oBGR, l_oCloud);

                if(l_bAddCloudSuccess)
                {
                    ++m_i32CurrentCloudNumber;
                    emit sendNumCloud(m_i32CurrentCloudNumber);

                    // retrieve total cloud
//                        deleteAndNullify(m_pCloudToDisplay);
//                        m_pCloudToDisplay = new swCloud::SWCloud();
//                        m_CAvatarPtr->totalCloud(*m_pCloudToDisplay);
                        l_pCloudToDisplay = new swCloud::SWCloud;
                        m_CAvatarPtr->totalCloud(*l_pCloudToDisplay);

                    // retrieve stasm points
                        std::vector<cv::Point2i> l_vP2IStasm;

                        if(m_bSendStasmPoints)
                        {
                            if(m_i32NumberStasm++ < m_i32MaxNumberStasm) // TODO : add mutex
                            {
                                m_CAvatarPtr->m_CStasmDetectPtr->featuresPoints(l_vP2IStasm);
                            }
                        }
                        emit sendStasmPoints(l_vP2IStasm);
                }
                else if(m_i32CurrentCloudNumber == 0)
                {
                    qWarning() << "The face must be detected when starting the recording. ";

                    // reset data
                        m_CAvatarPtr->resetData();
                    // stop
                        stopWork();
                        l_bContinueLoop = false;
                }

                if(m_i32CurrentCloudNumber > 0)
                {
                    // retrieve rectangles
                        deleteAndNullify(m_pCurrentFaceRect);
                        deleteAndNullify(m_pCurrentNoseRect);
                        m_pCurrentFaceRect = new cv::Rect(m_CAvatarPtr->lastRectFace());
                        m_pCurrentNoseRect = new cv::Rect(m_CAvatarPtr->lastRectNose());

                    // send the cloud and the rectangles to the interface for displaying
                        emit sendFaceRect(m_pCurrentFaceRect);
                        emit sendNoseRect(m_pCurrentNoseRect);

//                        if(m_pCloudToDisplay)
//                        {
//                            if(m_pCloudToDisplay->size() > 0)
//                            {
//                                emit sendCloud(m_pCloudToDisplay,false);
//                            }
//                        }
                        if(l_pCloudToDisplay)
                        {
                            if(l_pCloudToDisplay->size() > 2)
                            {
                                if(!l_bCamCloudInitialized)
                                {
                                    emit sendCloud(l_pCloudToDisplay, true);
                                    l_bCamCloudInitialized = true;
                                }
                                else
                                {
                                    emit sendCloud(l_pCloudToDisplay, false);
                                }
                            }
                        }
                }

//                if(m_CAvatarPtr->addCloudToAvatar(l_oBGR, l_oCloud))
//                {
//                    ++m_i32CurrentCloudNumber;
//                        emit sendNumCloud(m_i32CurrentCloudNumber);

//                    // retrieve total cloud
//                        deleteAndNullify(m_pCloudToDisplay);
//                        m_pCloudToDisplay = new swCloud::SWCloud();
//                        m_CAvatarPtr->totalCloud(*m_pCloudToDisplay);

                    // retrieve rectangles
//                        deleteAndNullify(m_pCurrentFaceRect);
//                        deleteAndNullify(m_pCurrentNoseRect);
//                        m_pCurrentFaceRect = new cv::Rect(m_CAvatarPtr->lastRectFace());
//                        m_pCurrentNoseRect = new cv::Rect(m_CAvatarPtr->lastRectNose());

//                    // retrieve stasm points
//                        std::vector<cv::Point2i> l_vP2IStasm;

//                        if(m_bSendStasmPoints)
//                        {
//                            if(m_i32NumberStasm++ < m_i32MaxNumberStasm) // TODO : add mutex
//                            {
//                                m_CAvatarPtr->m_CStasmDetectPtr->featuresPoints(l_vP2IStasm);
//                            }
//                        }
//                        emit sendStasmPoints(l_vP2IStasm);

//                    // send the cloud and the rectangles to the interface for displaying
//                        emit sendFaceRect(m_pCurrentFaceRect);
//                        emit sendNoseRect(m_pCurrentNoseRect);

//                        if(m_pCloudToDisplay->size() > 0)
//                        {
//                            emit sendCloud(m_pCloudToDisplay);
//                        }
//                }
            }
            else
            {
                // retrieve face texture
                    deleteAndNullify(m_pFaceTexture);
                    m_pFaceTexture = new cv::Mat();
                    *m_pFaceTexture = m_CAvatarPtr->m_oTextureMat.clone();
                    emit sendTexture(m_pFaceTexture);

                // construct avatar with the total cloud
                    m_CAvatarPtr->constructAvatar();

                // retrieve face mesh
                    deleteAndNullify(m_pFaceMeshResult);
                    m_pFaceMeshResult = new swMesh::SWMesh();
                    m_CAvatarPtr->lastResultFaceMesh(*m_pFaceMeshResult);

                // retrieve radial projection
                    deleteAndNullify(m_pRadialProjectionToDisplay);
                    m_pRadialProjectionToDisplay = new cv::Mat();
                    m_CAvatarPtr->lastRadialProjection(*m_pRadialProjectionToDisplay);

                // send results to interface
                    emit sendMesh(m_pFaceMeshResult, true);
                    emit sendMat(m_pRadialProjectionToDisplay);

                // change interface focus, enable/disable
                    emit switchTab(1);
                    emit enableInterfaceSignal();

                // stop
                    stopWork();
                    l_bContinueLoop = false;

            }
        }
    }

}


void SWCreateAvatarWorker::stopWork()
{
    m_oLoopMutex.lockForWrite();
        m_bDoWork        = false;
        m_i32NumberStasm = 0;
    m_oLoopMutex.unlock();

    emit stopWorkSignal();
}


void SWCreateAvatarWorker::reconstruct()
{
    // retrieve face texture
        deleteAndNullify(m_pFaceTexture);
        m_pFaceTexture = new cv::Mat();
        *m_pFaceTexture = m_CAvatarPtr->m_oTextureMat.clone();
        emit sendTexture(m_pFaceTexture);

    // construct avatar with the total cloud
        m_CAvatarPtr->constructAvatar();

    // retrieve face mesh
        deleteAndNullify(m_pFaceMeshResult);
        m_pFaceMeshResult = new swMesh::SWMesh();
        m_CAvatarPtr->lastResultFaceMesh(*m_pFaceMeshResult);

    // retrieve radial projection
        deleteAndNullify(m_pRadialProjectionToDisplay);
        m_pRadialProjectionToDisplay = new cv::Mat();
        m_CAvatarPtr->lastRadialProjection(*m_pRadialProjectionToDisplay);

    // send results to interface
        emit sendMesh(m_pFaceMeshResult, false);
        emit sendMat(m_pRadialProjectionToDisplay);

    // change interface focus, enable/disable
        emit switchTab(1);
        emit enableInterfaceSignal();
}

void SWCreateAvatarWorker::saveMeshFile(QString sPath)
{
    if(sPath.size() > 0)
    {
        int l_i32SeparatorsNb = 0;
        QString l_sName(sPath);
        for(int ii = 0; ii < sPath.size(); ++ii)
        {
            if(sPath[ii] == '/' || sPath[ii] == '\\')
            {
                ++l_i32SeparatorsNb;
            }
        }

        int ii = 0;
        while(ii < l_i32SeparatorsNb)
        {
            if(l_sName[0] == '/' || l_sName[0] == '\\')
            {
                ++ii;
            }

            l_sName.remove(0, 1);
        }

        QString l_sNameOBJ(l_sName);
        l_sName.remove(l_sName.size()-3, l_sName.size());

        QString l_sNameTexture(l_sName);
        l_sNameTexture.append("png");

        QString l_sNameMTL(l_sName);
        l_sNameMTL.append("mtl");

        QString l_sNameSTASM(l_sName);
        l_sNameSTASM.append("stasm");

        QString l_sPath(sPath);
        l_sPath.remove(l_sPath.size() - l_sNameOBJ.size(), l_sNameOBJ.size());

        qDebug() << "Path : "               << l_sPath;
        qDebug() << "Save OBJ file : "      << l_sNameOBJ;
        qDebug() << "Save texture file : "  << l_sNameTexture;
        qDebug() << "Save Material file : " << l_sNameMTL;

        m_pFaceMeshResult->saveToObj(l_sPath.toStdString(), l_sNameOBJ.toStdString(), l_sNameMTL.toStdString(), l_sNameTexture.toStdString());

        std::vector<int> l_vMesh_STASM_points_index = m_CAvatarPtr->mesh_stasm_points_index();
        if(l_vMesh_STASM_points_index.size() > 0)
        {
            qDebug() << "Save STASM file : " << l_sNameSTASM;

            std::ofstream l_oFlowSTASM;
            l_oFlowSTASM.open(l_sPath.toStdString() + l_sNameSTASM.toStdString());
//            l_oFlowSTASM << "# STASM file created with SWoOZ plateform (https://github.com/GuillaumeGibert/swooz)" << std::endl;

            for(uint ii = 0; ii < l_vMesh_STASM_points_index.size(); ++ii)
            {
                l_oFlowSTASM << ii << " " << l_vMesh_STASM_points_index[ii] << std::endl;
            }

            l_oFlowSTASM.close();
        }

        cv::imwrite((l_sPath + l_sNameTexture).toStdString() , *m_pFaceTexture);
    }
}

void SWCreateAvatarWorker::setCloudNumberValue(const int i32NumberOfClouds)
{
    m_oParametersMutex.lockForWrite();
    m_i32NumberOfClouds = i32NumberOfClouds;
    m_oParametersMutex.unlock();
}

void SWCreateAvatarWorker::setCalibrationXValue(const int i32CalibrationX)
{
    m_oParametersMutex.lockForWrite();
        m_i32CalibrationX = i32CalibrationX;
    m_oParametersMutex.unlock();

    m_pRGBDDeviceThread->setRecalibration(true, m_i32CalibrationX, m_i32CalibrationY);
}

void SWCreateAvatarWorker::setCalibrationYValue(const int i32CalibrationY)
{
    m_oParametersMutex.lockForWrite();
        m_i32CalibrationY = i32CalibrationY;
    m_oParametersMutex.unlock();

    m_pRGBDDeviceThread->setRecalibration(true, m_i32CalibrationX, m_i32CalibrationY);
}

void SWCreateAvatarWorker::resetKinect(const int i32VideoMode)
{
    m_pRGBDDeviceThread->stopListening();
    m_pRGBDDeviceThread->init(i32VideoMode);
    m_pRGBDDeviceThread->startListening();

    emit endResetKinect();
}

void SWCreateAvatarWorker::setExpandValue(const int i32Val)
{
    m_CAvatarPtr->setExpandValue(i32Val);
}

void SWCreateAvatarWorker::setExpandConnex(const int i32Val)
{
    m_CAvatarPtr->setExpandConnex(i32Val);
}

void SWCreateAvatarWorker::setEraseValue(const int i32Val)
{
    m_CAvatarPtr->setEraseValue(i32Val);
}

void SWCreateAvatarWorker::setEraseConnex(const int i32Val)
{
    m_CAvatarPtr->setEraseConnex(i32Val);
}

void SWCreateAvatarWorker::setDepthCloud(const double dVal)
{
    m_CAvatarPtr->setDepthCloud((float)dVal);
}

void SWCreateAvatarWorker::addPointToDeleteRadialProj(const QPoint oPtToAdd, const QSize oSize, const bool leftClick)
{
    std::vector<int> l_vPt(2);
    l_vPt[0] = oPtToAdd.x() / (oSize.width() *1.f / m_CAvatarPtr->m_i32WidthRadialProj);
    l_vPt[1] = oPtToAdd.y() / (oSize.height()*1.f / m_CAvatarPtr->m_i32HeightRadialProj );
    m_CAvatarPtr->addPixelToDelete(l_vPt);
}

void SWCreateAvatarWorker::resetDeletedPointsRadialProj()
{
    m_CAvatarPtr->resetDeletedPointsRadialProj();
}

void SWCreateAvatarWorker::setWidthRectRatio(const int i32Val)
{
//    m_CAvatarPtr->set
}

void SWCreateAvatarWorker::setHeightRectRatio(const int i32Val)
{
//    m_CAvatarPtr->set
}

void SWCreateAvatarWorker::setRadialWidth(const int i32Val)
{
    m_CAvatarPtr->setWidthRadialProjection(i32Val);
}

void SWCreateAvatarWorker::setRadialHeight(const int i32Val)
{
    m_CAvatarPtr->setHeightRadialProjection(i32Val);
}

void SWCreateAvatarWorker::setBilateralDiameter(const int i32Val)
{
    m_CAvatarPtr->setBilateralFilter(i32Val);
}

void SWCreateAvatarWorker::setBilateralColor(const int i32Val)
{
    m_CAvatarPtr->setColorFilterValue(i32Val);
}

void SWCreateAvatarWorker::setBilateralSpace(const int i32Val)
{
    m_CAvatarPtr->setSpaceFilterValue(i32Val);
}

void SWCreateAvatarWorker::setCylinderRadius(const double dRadius)
{
    m_CAvatarPtr->setCylinderRadiusValue((float)dRadius);
}

void SWCreateAvatarWorker::setUseBilateralFilter(const bool bUseFilter)
{
    m_CAvatarPtr->setUseBilateralFilter(bUseFilter);
}

void SWCreateAvatarWorker::setUseStasm(const bool bUseStasm)
{
    m_CAvatarPtr->setUseStasm(bUseStasm);
    m_bSendStasmPoints = bUseStasm;
}

void SWCreateAvatarWorker::setErode(const int i32Erode)
{
    m_CAvatarPtr->setErodeValue(i32Erode);
}

void SWCreateAvatarWorker::setDilate(const int i32Dilate)
{
    m_CAvatarPtr->setDilateValue(i32Dilate);
}
