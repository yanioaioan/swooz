
/**
 * \file SWIcubHead.cpp
 * \author Florian Lance
 * \date 23-04-2014
 * \brief Defines SWIcubHead
 */


#include <sstream>

#include "geometryUtility.h"
#include "SWTrackingDevice.h"

#include "icub/SWIcubHead.h"
//#include "icub/SWiCubFaceMotion.h"


using namespace yarp::os;


swTeleop::SWIcubHead::SWIcubHead() : m_bInitialized(false), m_bIsRunning(false), m_i32HeadJointsNb(6), m_dHeadTimeLastBottle(-1.), m_dGazeTimeLastBottle(-1.),
                                     m_pIHeadVelocity(NULL), m_pIHeadEncoders(NULL), m_pIHeadPosition(NULL), m_pVelocityController(NULL)
{        
    // set ini file defaults values
        // parts to be activated
            m_bHeadActivatedDefault = 1;
            m_bGazeActivatedDefault = 1;
            m_bLEDActivatedDefault  = 1;

        // timeout
            m_i32TimeoutHeadResetDefault = 3000;
            m_i32TimeoutGazeResetDefault = 3000;
            m_i32TimeoutLEDResetDefault  = 3000;

        // eyeslids
            m_dMinEyelidsDefault    = 30.;
            m_dMaxEyelidsDefault    = 70.;
            m_dMinEyelidsSimDefault = 0.;
            m_dMaxEyelidsSimDefault = 70.;

        // accelerations / speeds
            m_dVelocityToleranceDefault = 15.;
            double l_aDMinJointDefault[]                        = {-40.,-70.,-55.};
            double l_aDMaxJointDefault[]                        = { 30., 60., 50.};
            double l_aDHeadJointVelocityDefault[]               = {50.,50.,50.,50.,50.,50.};
            double l_aDHeadJointVelocityKDefault[]              = {0.9,0.9,0.9,0.9,0.9,0.9};
            double l_aDHeadJointPositionAccelerationDefault[]   = {50.,50.,50.,50.,50.,50.};
            double l_aDHeadJointPositionSpeedDefault[]          = {50.,50.,50.,50.,50.,50.};            
            m_vHeadMinJointDefault                  = std::vector<double>(l_aDMinJointDefault, l_aDMinJointDefault + sizeof(l_aDMinJointDefault) / sizeof(double));
            m_vHeadMaxJointDefault                  = std::vector<double>(l_aDMaxJointDefault, l_aDMaxJointDefault + sizeof(l_aDMaxJointDefault) / sizeof(double));
            m_vHeadJointVelocityKDefault            = std::vector<double>(l_aDHeadJointVelocityKDefault, l_aDHeadJointVelocityKDefault + sizeof(l_aDHeadJointVelocityKDefault) / sizeof(double));
            m_vHeadJointVelocityAccelerationDefault = std::vector<double>(l_aDHeadJointVelocityDefault, l_aDHeadJointVelocityDefault + sizeof(l_aDHeadJointVelocityDefault) / sizeof(double));            
            m_vHeadJointPositionAccelerationDefault = std::vector<double>(l_aDHeadJointPositionAccelerationDefault, l_aDHeadJointPositionAccelerationDefault + sizeof(l_aDHeadJointPositionAccelerationDefault) / sizeof(double));
            m_vHeadJointPositionSpeedDefault        = std::vector<double>(l_aDHeadJointPositionSpeedDefault, l_aDHeadJointPositionSpeedDefault + sizeof(l_aDHeadJointPositionSpeedDefault) / sizeof(double));

            m_vHeadMinJoint = std::vector<double>(m_vHeadMinJointDefault.size());
            m_vHeadMaxJoint = std::vector<double>(m_vHeadMaxJointDefault.size());
            m_vHeadJointVelocityAcceleration = std::vector<double>(m_vHeadJointVelocityAccelerationDefault.size());
            m_vHeadJointVelocityK            = std::vector<double>(m_vHeadJointVelocityKDefault.size());
            m_vHeadJointPositionAcceleration = std::vector<double>(m_vHeadJointPositionAccelerationDefault.size());
            m_vHeadJointPositionSpeed = std::vector<double>(m_vHeadJointPositionSpeedDefault.size());
}

swTeleop::SWIcubHead::~SWIcubHead()
{
    while(m_pVelocityController->isRunning())
    {
        yarp::os::Time::delay(0.01);
    }

    deleteAndNullify(m_pVelocityController);
}

bool swTeleop::SWIcubHead::init( yarp::os::ResourceFinder &oRf)
{
    if(m_bInitialized)
    {
        std::cerr << "Icub head is already initialized. " << std::endl;
        return true;
    }

    // gets the module name which will form the stem of all module port names
        m_sModuleName   = oRf.check("name", Value("teleoperation_iCub"), "Teleoperation/iCub Module name (string)").asString();
        m_sRobotName    = oRf.check("robot",Value("icubSim"),  "Robot name (string)").asString();

    // robot parts to control
        m_bHeadActivated = oRf.check("headActivated", Value(m_bHeadActivatedDefault), "Head activated (int)").asInt() != 0;
        m_bGazeActivated = oRf.check("gazeActivated", Value(m_bGazeActivatedDefault), "Gaze activated (int)").asInt() != 0;
        m_bLEDActivated  = oRf.check("LEDSActivated", Value(m_bLEDActivatedDefault), "LEDS activated (int)"). asInt() != 0;

    // min / max values for iCub head joints
        for(uint ii = 0; ii < m_vHeadJointVelocityAcceleration.size(); ++ii)
        {
            std::ostringstream l_os;
            l_os << ii;

            if(ii < m_vHeadMinJointDefault.size())
            {
                std::string l_sMinJoint("headMinValueJoint" + l_os.str());
                std::string l_sMaxJoint("headMaxValueJoint" + l_os.str());

                std::string l_sMinJointInfo("Head minimum joint" + l_os.str() + " Value (double)");
                std::string l_sMaxJointInfo("Head maximum joint" + l_os.str() + " Value (double)");

                m_vHeadMinJoint[ii] = oRf.check(l_sMinJoint.c_str(), m_vHeadMinJointDefault[ii], l_sMinJointInfo.c_str()).asDouble();
                m_vHeadMaxJoint[ii] = oRf.check(l_sMaxJoint.c_str(), m_vHeadMaxJointDefault[ii], l_sMaxJointInfo.c_str()).asDouble();
            }

            std::string l_sHeadJointVelocityAcceleration("headJointVelocityAcceleration" + l_os.str());
            std::string l_sHeadJointVelocityK("headJointVelocityK" + l_os.str());
            std::string l_sHeadJointPositionAcceleration("headJointPositionAcceleration" + l_os.str());
            std::string l_sHeadJointPositionSpeed("headJointPositionSpeed" + l_os.str());

            std::string l_sHeadJointVelocityAccelerationInfo("Head joint velocity acceleration " + l_os.str() + " Value (double)");
            std::string l_sHeadJointVelocityKInfo("Head joint velocity K coeff"+ l_os.str() + " Value (double)");
            std::string l_sHeadJointPositionAccelerationInfo("Head joint position acceleration " + l_os.str() + " Value (double)");
            std::string l_sHeadJointPositionSpeedInfo("Head joint position speed " + l_os.str() + " Value (double)");

            m_vHeadJointVelocityAcceleration[ii]= oRf.check(l_sHeadJointVelocityAcceleration.c_str(), m_vHeadJointVelocityAccelerationDefault[ii], l_sHeadJointVelocityAccelerationInfo.c_str()).asDouble();
            m_vHeadJointPositionAcceleration[ii]= oRf.check(l_sHeadJointPositionAcceleration.c_str(), m_vHeadJointPositionAccelerationDefault[ii], l_sHeadJointPositionAccelerationInfo.c_str()).asDouble();
            m_vHeadJointPositionSpeed[ii]       = oRf.check(l_sHeadJointPositionSpeed.c_str(),        m_vHeadJointPositionSpeedDefault[ii],        l_sHeadJointPositionSpeedInfo.c_str()).asDouble();
            m_vHeadJointVelocityK[ii]           = oRf.check(l_sHeadJointVelocityK.c_str(),            m_vHeadJointVelocityKDefault[ii],            l_sHeadJointVelocityKInfo.c_str()).asDouble();
            std::cout << m_vHeadJointVelocityK[ii] << " ";
        }

    // min / max values for iCub eyelids
        if(m_sRobotName == "icubSim")
        {
            m_dMinEyelids = oRf.check("minValueEyelidsSim",  Value( m_dMinEyelidsSimDefault ),  "Min value eyelidSim (double)").asDouble();
            m_dMaxEyelids = oRf.check("maxValueEyelidsSim",  Value( m_dMaxEyelidsSimDefault ),  "Max value eyelidSim (double)").asDouble();
        }
        else
        {
            m_dMinEyelids = oRf.check("minValueEyelids",  Value( m_dMinEyelidsDefault ),  "Min value eyelid (double)").asDouble();
            m_dMaxEyelids = oRf.check("maxValueEyelids",  Value( m_dMaxEyelidsDefault ),  "Max value eyelid (double)").asDouble();
        }
    // miscellaneous
        m_i32TimeoutHeadReset  = oRf.check("headTimeoutReset",   Value(m_i32TimeoutHeadResetDefault), "Head timeout reset iCub (int)").asInt();
        m_i32TimeoutGazeReset  = oRf.check("gazeTimeoutReset",   Value(m_i32TimeoutGazeResetDefault), "Gaze timeout reset iCub (int)").asInt();
        m_i32TimeoutLEDReset   = oRf.check("LEDTimeoutReset",    Value(m_i32TimeoutLEDResetDefault), "LED display timeout reset iCub (int)").asInt();
        m_dVelocityTolerance   = oRf.check("velocityTolerance",  Value(m_dVelocityToleranceDefault), "Velocity tolerance (double)").asDouble();

    // set polydriver options
        m_oHeadOptions.put("robot",     m_sRobotName.c_str());
        m_oHeadOptions.put("device",    "remote_controlboard");
        m_oHeadOptions.put("local",    ("/local/" + m_sRobotName + "/head").c_str());
        m_oHeadOptions.put("name",     ("/" + m_sRobotName + "/head").c_str());
        m_oHeadOptions.put("remote",   ("/" + m_sRobotName + "/head").c_str());


    // init polydriver
        m_oRobotHead.open(m_oHeadOptions);
        if(!m_oRobotHead.isValid())
        {
            std::cerr << "-ERROR: robotHead is not valid" << std::endl;
            return (m_bInitialized=false);
        }

    // initializing controllers
        if (!m_oRobotHead.view(m_pIHeadVelocity) || !m_oRobotHead.view(m_pIHeadPosition) || !m_oRobotHead.view(m_pIHeadEncoders))
        {
            std::cerr << "-ERROR: while getting required robot head interfaces." << std::endl;
            m_oRobotHead.close();
            return (m_bInitialized=false);
        }

    // init ports
        m_sHeadTrackerPortName  = "/teleoperation/" + m_sRobotName + "/head";
        m_sGazeTrackerPortName  = "/teleoperation/" + m_sRobotName + "/gaze";
        m_sFaceTrackerPortName  = "/teleoperation/" + m_sRobotName + "/face";
        m_sEyelidInputPortName  = "/" + m_sRobotName + "/face/raw/in";
        m_sEyelidOutputPortName = "/teleoperation/" + m_sRobotName + "/eyelids/out";

    // open ports
        bool l_bPortOpeningSuccess = true;
        if(m_bHeadActivated)
        {
            l_bPortOpeningSuccess = m_oHeadTrackerPort.open(m_sHeadTrackerPortName.c_str());
        }
        if(m_bGazeActivated && l_bPortOpeningSuccess)
        {
            l_bPortOpeningSuccess = m_oGazeTrackerPort.open(m_sGazeTrackerPortName.c_str());
        }
        if(m_bLEDActivated && l_bPortOpeningSuccess)
        {
            l_bPortOpeningSuccess = m_oFaceTrackerPort.open(m_sFaceTrackerPortName.c_str());
        }
        if((m_bLEDActivated || m_bGazeActivated) && l_bPortOpeningSuccess)
        {
            l_bPortOpeningSuccess = m_oFaceHandlerPort.open(m_sEyelidOutputPortName.c_str());
        }

        if(!l_bPortOpeningSuccess)
        {
            std::cerr << "-ERROR: Unable to open ports." << std::endl;
            m_oRobotHead.close();
            return (m_bInitialized=false);
        }

    //  attach to port
        if(m_bLEDActivated)
        {
            yarp::os::Network::connect(m_sEyelidOutputPortName.c_str(), m_sEyelidInputPortName.c_str());
        }

    // retrieve head number of joints
        m_pIHeadPosition->getAxes(&m_i32HeadJointsNb);

    // set accelerations/speeds
        for(int ii = 0; ii < m_i32HeadJointsNb; ++ii)
        {
            m_pIHeadPosition->setRefAcceleration(ii, m_vHeadJointPositionAcceleration[ii]);
            m_pIHeadPosition->setRefSpeed(ii, m_vHeadJointPositionSpeed[ii]);
            m_pIHeadVelocity->setRefAcceleration(ii, m_vHeadJointVelocityAcceleration[ii]);
        }

    // init controller
        m_pVelocityController = new SWVelocityController(m_pIHeadEncoders, m_pIHeadVelocity, m_vHeadJointVelocityK, m_dVelocityTolerance, 10);
        m_pVelocityController->enableHead(m_bHeadActivated);
        m_pVelocityController->enableGaze(m_bGazeActivated);

    return (m_bIsRunning=m_bInitialized=true);
}

bool swTeleop::SWIcubHead::checkBottles()
{
    if(!m_bIsRunning)
    {
        return false;
    }

    if(!m_bInitialized)
    {
        std::cerr << "Icub head control module not initialized. " << std::endl;
        return (m_bIsRunning=false);
    }

    // init command vector
        yarp::sig::Vector l_vHeadJoints;
        l_vHeadJoints.resize(m_i32HeadJointsNb);
        l_vHeadJoints = 0.;

    // defines bottles
        Bottle *l_pHeadTarget = NULL, *l_pFaceTarget = NULL, *l_pGazeTarget = NULL;

    // read head command
        if(m_bHeadActivated)
        {
            l_pHeadTarget = m_oHeadTrackerPort.read(false);

            if(l_pHeadTarget)
            {
                int l_deviceId = l_pHeadTarget->get(0).asInt();

                switch(l_deviceId)
                {
                    case swTracking::FOREST_LIB :
                        {
                            l_vHeadJoints[0] = -l_pHeadTarget->get(1).asDouble(); //head rotation "yes" [-40 30]
                            l_vHeadJoints[1] = -l_pHeadTarget->get(3).asDouble(); //head rotation [-70 60]
                            l_vHeadJoints[2] = -l_pHeadTarget->get(2).asDouble(); //head rotation "no" [-55 55]
                        }
                    break;
                    case swTracking::COREDATA_LIB :
                        {
                            l_vHeadJoints[0] =  swUtil::rad2Deg(l_pHeadTarget->get(4).asDouble()); // up/down head
                            l_vHeadJoints[1] = -swUtil::rad2Deg(l_pHeadTarget->get(6).asDouble()); // left/right head
                            l_vHeadJoints[2] =  swUtil::rad2Deg(l_pHeadTarget->get(5).asDouble()); // head
                        }
                    break;
                    case swTracking::EMICP_LIB :
                        {
                            l_vHeadJoints[0] = -l_pHeadTarget->get(4).asDouble(); // up/down head
                            l_vHeadJoints[1] = -l_pHeadTarget->get(6).asDouble(); // left/right head
                            l_vHeadJoints[2] = -l_pHeadTarget->get(5).asDouble(); // head
                        }
                    break;
                    case swTracking::FACESHIFT_LIB :
                        {
                            l_vHeadJoints[0] = -swUtil::rad2Deg(l_pHeadTarget->get(4).asDouble()); // up/down head
                            l_vHeadJoints[1] = -swUtil::rad2Deg(l_pHeadTarget->get(6).asDouble()); // left/right head
                            l_vHeadJoints[2] = -swUtil::rad2Deg(l_pHeadTarget->get(5).asDouble()); // head
                        }
                    break;
                    case swTracking::OPENNI_LIB :
                    {
                        std::vector<double> l_pointNeck(3), l_pointHead(3), l_pointLShoulder(3), l_pointRShoulder(3);
                        l_pointNeck[0] = l_pHeadTarget->get(1).asDouble();
                        l_pointNeck[1] = l_pHeadTarget->get(2).asDouble();
                        l_pointNeck[2] = l_pHeadTarget->get(3).asDouble();
                        l_pointHead[0] = l_pHeadTarget->get(4).asDouble();
                        l_pointHead[1] = l_pHeadTarget->get(5).asDouble();
                        l_pointHead[2] = l_pHeadTarget->get(6).asDouble();
                        l_pointLShoulder[0] = l_pHeadTarget->get(7).asDouble();
                        l_pointLShoulder[1] = l_pHeadTarget->get(8).asDouble();
                        l_pointLShoulder[2] = l_pHeadTarget->get(9).asDouble();
                        l_pointRShoulder[0] = l_pHeadTarget->get(10).asDouble();
                        l_pointRShoulder[1] = l_pHeadTarget->get(11).asDouble();
                        l_pointRShoulder[2] = l_pHeadTarget->get(12).asDouble();

                        std::vector<double> l_vecClavicles  = swUtil::vec(l_pointLShoulder,	l_pointRShoulder);
                        std::vector<double> l_vecHead       = swUtil::vec(l_pointNeck,		l_pointHead);
                        std::vector<double> l_rpyHead = swUtil::computeRollPitchYaw(l_vecHead, l_vecClavicles);

                        l_vHeadJoints[0] = -l_rpyHead[1];
                        l_vHeadJoints[1] = -l_rpyHead[0];
                        l_vHeadJoints[2] =  l_rpyHead[2];
                    }
                    break;
                }

                m_dHeadTimeLastBottle = -1.;
                m_pVelocityController->enableHead(true);
            }
            else // manage timeout and reset position
            {
                if(m_dHeadTimeLastBottle < 0.)
                {
                    m_dHeadTimeLastBottle = yarp::os::Time::now();
                }
                else
                {
                    if(yarp::os::Time::now() - m_dHeadTimeLastBottle > 0.001 * m_i32TimeoutHeadReset)
                    {
                        m_pVelocityController->enableHead(false);
                        resetHeadPosition();
                        m_dHeadTimeLastBottle = -1.;
                    }
                }
            }
        }

    // read gaze commands
        if(m_bGazeActivated)
        {
            l_pGazeTarget = m_oGazeTrackerPort.read(false);

            if(l_pGazeTarget)
            {
                int l_deviceId = l_pGazeTarget->get(0).asInt();

                switch(l_deviceId)
                {
                    case swTracking::COREDATA_LIB :
                        {
                            // eye position
                                l_vHeadJoints[3] = swUtil::rad2Deg( (l_pGazeTarget->get(9) .asDouble() + l_pGazeTarget->get(14).asDouble())/2.); // up/down eye [-35; +15]
                                l_vHeadJoints[4] = swUtil::rad2Deg(-(l_pGazeTarget->get(10).asDouble() + l_pGazeTarget->get(15).asDouble())/2.); // version angle [-50; 52] = (L+R)/2
                                l_vHeadJoints[5] = swUtil::rad2Deg( -l_pGazeTarget->get(10).asDouble() + l_pGazeTarget->get(15).asDouble());     // vergence angle [0 90] = R-L

                            // eye closure
                                Bottle &l_oFaceMotionBottle = m_oFaceHandlerPort.prepare();
                                l_oFaceMotionBottle.clear();
                                double l_dLeftEyeClosure = l_pGazeTarget->get(8).asDouble(), l_dRightEyeClosure = l_pGazeTarget->get(13).asDouble();
                                l_oFaceMotionBottle.addString(eyesOpeningCode((1.0-(l_dLeftEyeClosure + l_dRightEyeClosure)/2.0), m_dMinEyelids, m_dMaxEyelids).c_str());
                                m_oFaceHandlerPort.write();
                        }
                    break;
                }

                m_dGazeTimeLastBottle = -1.;
                m_pVelocityController->enableGaze(true);
            }
            else // manage timeout and reset position
            {
                if(m_dGazeTimeLastBottle < 0.)
                {
                    m_dGazeTimeLastBottle = yarp::os::Time::now();
                }
                else
                {
                    if(yarp::os::Time::now() - m_dGazeTimeLastBottle > 0.001 * m_i32TimeoutGazeReset)
                    {
                        m_pVelocityController->enableGaze(false);
                        resetGazePosition();
                        m_dGazeTimeLastBottle = -1.;
                    }
                }
            }
        }


    // read face commands
        if(m_bLEDActivated)
        {
            l_pFaceTarget = m_oFaceTrackerPort.read(false);

            if(l_pFaceTarget)
            {
                int l_deviceId = l_pFaceTarget->get(0).asInt();

                switch(l_deviceId)
                {
                    case swTracking::COREDATA_LIB :
                        {
                            Bottle &l_oFaceMotionBottle = m_oFaceHandlerPort.prepare();
                            // retrieve values
                                // eyebrows
                                std::vector<double> l_vLeftEyeBrowPoints, l_vRightEyeBrowPoints, l_vLeftEyeCenter, l_vRightEyeCenter;
                                for(int ii = 0; ii < 9; ++ii)
                                {
                                    l_vLeftEyeBrowPoints.push_back(l_pFaceTarget->get(52+ii).asDouble());
                                    l_vRightEyeBrowPoints.push_back(l_pFaceTarget->get(43+ii).asDouble());

                                    if(ii < 3)
                                    {
                                        l_vLeftEyeCenter.push_back(l_pFaceTarget->get(64+ii).asDouble());
                                        l_vRightEyeCenter.push_back(l_pFaceTarget->get(67+ii).asDouble());
                                    }
                                }

                                // mouth
                                std::vector<double> l_vInnerLip2, l_vInnerLip6;
                                for(int ii = 0; ii < 3; ++ii)
                                {
                                    l_vInnerLip2.push_back(l_pFaceTarget->get(25+ii).asDouble());
                                    l_vInnerLip6.push_back(l_pFaceTarget->get(37+ii).asDouble());
                                }




////                            if(m_bFirstLEDCommand)
////                            {
////                                m_oIcubFaceLEDCmd.setNeutralPoints(l_vLeftEyeBrowPoints, l_vRightEyeBrowPoints, l_vLeftEyeCenter, l_vRightEyeCenter);
////                                m_bFirstLEDCommand = false;
////                            }



////                                std::string l_sNewMouth         = m_oIcubFaceLEDCmd.lipCommand(l_vInnerLip2, l_vInnerLip6);
////                                std::string l_sNewLeftEyebrow   = m_oIcubFaceLEDCmd.leftEyeBrowCommand(l_vLeftEyeBrowPoints, l_vLeftEyeCenter);
////                                std::string l_sNewRightEyebrow  = m_oIcubFaceLEDCmd.rightEyeBrowCommand(l_vRightEyeBrowPoints, l_vRightEyeCenter);

//                                //if(m_sPreviousMouth != l_sNewMouth)
//                                {
//                                    l_oFaceMotionBottle.clear();
//                                    l_oFaceMotionBottle.addString(l_sNewMouth.c_str());
//                                    m_oFaceHandlerPort.write();
//                                    //m_sPreviousMouth = l_sNewMouth;
//                                }

//                                //if(m_sPreviousLeftEyebrow != l_sNewLeftEyebrow)
//                                if(false)
//                                {
//                                    l_oFaceMotionBottle.clear();
//                                    l_oFaceMotionBottle.addString(l_sNewLeftEyebrow.c_str());
//                    //					l_oFaceMotionBottle.addString(std::string("L02").c_str());
//                                    m_oFaceHandlerPort.write();
//                                    m_sPreviousLeftEyebrow = l_sNewLeftEyebrow;
//                                    //Time::delay(0.001);
//                                }

//                                //if(m_sPreviousRightEyebrow != l_sNewRightEyebrow)
//                                if(false)
//                                {
//                                    l_oFaceMotionBottle.clear();
//                                    l_oFaceMotionBottle.addString(l_sNewRightEyebrow.c_str());
//                    //					l_oFaceMotionBottle.addString(std::string("R02").c_str());
//                                    m_oFaceHandlerPort.write();
//                                    m_sPreviousRightEyebrow = l_sNewRightEyebrow;
//                                    //Time::delay(0.001);
//                                }
                        }
                    break;
                }
            }
            else
            {

            }
        }

    // check each joint value to ensure it is in the right range, if not crop to the max/min values
        for(uint ii = 0; ii < l_vHeadJoints.size(); ++ii)
        {
            if(l_vHeadJoints[ii] < m_vHeadMinJoint[ii])
            {
                l_vHeadJoints[ii] = m_vHeadMinJoint[ii];
            }
            if(l_vHeadJoints[ii] > m_vHeadMaxJoint[ii])
            {
                l_vHeadJoints[ii] = m_vHeadMaxJoint[ii];
            }
        }

        if(l_pHeadTarget || l_pGazeTarget)
        {
            m_pVelocityController->setJoints(l_vHeadJoints);

            if(!m_pVelocityController->isRunning())
            {
                m_pVelocityController->start(); // TODO : check
            }
        }

    return true;
}


void swTeleop::SWIcubHead::resetHeadPosition()
{
    if(m_bHeadActivated)
    {
        m_pIHeadPosition->positionMove(0,0.);
        m_pIHeadPosition->positionMove(1,0.);
        m_pIHeadPosition->positionMove(2,0.);
    }
}

void swTeleop::SWIcubHead::resetGazePosition()
{
    if(m_bGazeActivated)
    {
        m_pIHeadPosition->positionMove(3,0.);
        m_pIHeadPosition->positionMove(4,0.);
        m_pIHeadPosition->positionMove(5,0.);
    }
}


bool swTeleop::SWIcubHead::close()
{

    bool l_bHeadPositionCloseState = m_pIHeadPosition->stop();
    bool l_bRobotHeadCloseState    = m_oRobotHead.close();

    if(m_pVelocityController->isRunning())
    {
        m_pVelocityController->stop();
    }

    // close ports
        if(m_bHeadActivated)
        {
            m_oHeadTrackerPort.close();
        }
        if(m_bGazeActivated)
        {
            m_oGazeTrackerPort.close();
        }
        if(m_bLEDActivated)
        {
            m_oFaceTrackerPort.close();
            m_oFaceHandlerPort.close();
        }

    return (l_bHeadPositionCloseState && l_bRobotHeadCloseState);
}

bool swTeleop::SWIcubHead::interruptModule()
{
    m_bIsRunning = false;

    // reset positions
        if(m_bHeadActivated)
        {
            resetHeadPosition();
        }
        if(m_bGazeActivated)
        {
            resetGazePosition();
        }


    if(m_pVelocityController->isRunning())
    {
        m_pVelocityController->stop();
    }

    // close ports
        if(m_bHeadActivated)
        {
            m_oHeadTrackerPort.interrupt();
        }
        if(m_bGazeActivated)
        {
            m_oGazeTrackerPort.interrupt();
        }
        if(m_bLEDActivated)
        {
            m_oFaceTrackerPort.interrupt();
            m_oFaceHandlerPort.interrupt();
        }

    std::cout << "--Interrupting the iCub Head Teleoperation module..." << std::endl;

    return true;
}

std::string swTeleop::SWIcubHead::eyesOpeningCode(cdouble dEyeLids, cdouble dMinValue, cdouble dMaxValue)
{
    int l_i32ScaledValue = (int)(dMinValue + (dMaxValue - dMinValue) * dEyeLids);

    std::ostringstream l_osCodeValue;
    l_osCodeValue << 'S';
    l_osCodeValue << l_i32ScaledValue;

    return l_osCodeValue.str();
}

swTeleop::SWVelocityController::SWVelocityController(yarp::dev::IEncoders *pIHeadEncoders, yarp::dev::IVelocityControl *pIHeadVelocity,
                                                     std::vector<double> &vHeadJointVelocityK, double dVelocityTolerance, int i32Rate)
    : RateThread(i32Rate), m_dVelocityTolerance(dVelocityTolerance), m_bHeadEnabled(false), m_bGazeEnabled(false) , m_vHeadJointVelocityK(vHeadJointVelocityK)
{   
    if(pIHeadEncoders)
    {
        m_pIHeadEncoders = pIHeadEncoders;
    }
    if(pIHeadVelocity)
    {
        m_pIHeadVelocity = pIHeadVelocity;
    }    
}

void swTeleop::SWVelocityController::run()
{
    double l_dToterance = DBL_MAX;

    while(l_dToterance > m_dVelocityTolerance)
    {
        m_oMutex.lock();
            bool l_bHeadEnabled = m_bHeadEnabled;
            bool l_bGazeEnabled = m_bGazeEnabled;
            yarp::sig::Vector l_vHeadJoints = m_vLastHeadJoint;
        m_oMutex.unlock();

        yarp::sig::Vector l_vEncoders, l_vCommand;
        l_vEncoders.resize(l_vHeadJoints.size());
        l_vCommand.resize(l_vHeadJoints.size());

        m_pIHeadEncoders->getEncoders(l_vEncoders.data());

        // head rotation / gaze
            for(uint ii = 0; ii < l_vCommand.size(); ++ii)
            {
                l_vCommand[ii] = m_vHeadJointVelocityK[ii] * (l_vHeadJoints[ii] - l_vEncoders[ii]);
            }

        // velocity move
        //  head
            if(l_bHeadEnabled)
            {
                for(int ii = 0; ii < 3; ++ii)
                {
                    m_pIHeadVelocity->velocityMove(ii, l_vCommand[ii]);
                }
            }
        //  gaze
            if(l_bGazeEnabled)
            {
                for(int ii = 3; ii < 6; ++ii)
                {
                    m_pIHeadVelocity->velocityMove(ii, l_vCommand[ii]);
                }
            }

        // compute tolerance
            l_dToterance = 0;

            for(uint ii = 0; ii < l_vCommand.size(); ++ii)
            {
                l_dToterance += sqrt(l_vCommand[ii]*l_vCommand[ii]);
            }
    }
}


void swTeleop::SWVelocityController::enableHead(cbool bActivated)
{
    m_oMutex.lock();
        m_bHeadEnabled = bActivated;
    m_oMutex.unlock();
}

void swTeleop::SWVelocityController::enableGaze(cbool bActivated)
{
    m_oMutex.lock();
        m_bGazeEnabled = bActivated;
    m_oMutex.unlock();
}

void swTeleop::SWVelocityController::setJoints(const yarp::sig::Vector &vJoints)
{
    m_oMutex.lock();
        m_vLastHeadJoint = vJoints;
    m_oMutex.unlock();
}


std::string swTeleop::SWIcubFaceLabLEDCommand::leftEyeBrowCommand(const std::vector<double> &a3DLeftEyeBrow)
{
    std::string l_sEyeBrowCommand = eyeBrowCommand(a3DLeftEyeBrow, true);

    if(l_sEyeBrowCommand.size() == 0)
    {
        return std::string("");
    }

    return std::string("L") + l_sEyeBrowCommand;
}

std::string swTeleop::SWIcubFaceLabLEDCommand::rightEyeBrowCommand(const std::vector<double> &a3DRightEyeBrow)
{
   std::string l_sEyeBrowCommand = eyeBrowCommand(a3DRightEyeBrow, false);

   if(l_sEyeBrowCommand.size() == 0)
   {
       return std::string("");
   }

   return std::string("R") + l_sEyeBrowCommand;
}


std::string swTeleop::SWIcubFaceLabLEDCommand::eyeBrowCommand(const std::vector<double> &a3DEyeBrow, const bool bLeftEye)
{
    double l_dXMedian = 0.0, l_dYMedian = 0.0;

    for(uint ii = 0; ii < a3DEyeBrow.size()/3; ++ii)
    {
        l_dXMedian += a3DEyeBrow[ii*3];
        l_dYMedian += a3DEyeBrow[ii*3+1];
    }

    l_dXMedian /= 3.0; l_dYMedian /= 3.0;

    double l_dEyeSquareDist  = (a3DEyeBrow[0] - l_dXMedian)*(a3DEyeBrow[0] - l_dXMedian) + (a3DEyeBrow[1] - l_dYMedian)*(a3DEyeBrow[1] - l_dYMedian);
    l_dEyeSquareDist = sqrt(l_dEyeSquareDist);
    bool l_bDown, l_bNeutral, l_bUp1, l_bUp2;

    if(bLeftEye)
    {
        l_bDown     = l_dEyeSquareDist < m_dLeftThresholdDown;
        l_bNeutral  = l_dEyeSquareDist < m_dLeftThresholdNeutral;
        l_bUp1      = l_dEyeSquareDist < m_dLeftThresholdUp1;
        l_bUp2      = l_dEyeSquareDist < m_dLeftThresholdUp2;
    }
    else
    {
        l_bDown     = l_dEyeSquareDist < m_dRightThresholdDown;
        l_bNeutral  = l_dEyeSquareDist < m_dRightThresholdNeutral;
        l_bUp1      = l_dEyeSquareDist < m_dRightThresholdUp1;
        l_bUp2      = l_dEyeSquareDist < m_dRightThresholdUp2;
    }

    if(l_bDown)
    {
        return std::string("01");
    }
    if(l_bNeutral)
    {
        return std::string("02");
    }
    if(l_bUp1)
    {
        return std::string("03");
    }
    if(l_bUp2)
    {
        return std::string("04");
    }

    std::cerr << "Bad values input points eyeBrowCommand : "<< l_dEyeSquareDist << std::endl;

    return std::string("02");
}

std::string swTeleop::SWIcubFaceLabLEDCommand::lipCommand(const std::vector<double> &a3DLip1, const std::vector<double> &a3DLip2)
{
    if(a3DLip1.size() != a3DLip2.size() && a3DLip1.size() != 3)
    {
        std::cerr << "Error size input vectors : lipCommand. " << std::endl;
        return std::string("M0A");
    }

    double l_dLipsDistance = sqrt( pow(a3DLip2[0] - a3DLip1[0], 2) + pow(a3DLip2[1] - a3DLip1[1], 2) + pow(a3DLip2[2] - a3DLip1[2], 2));


    if(l_dLipsDistance > m_dThresholdMouth)
    {
        return std::string("M16");
    }
    else
    {
        return std::string("M08");
    }
}