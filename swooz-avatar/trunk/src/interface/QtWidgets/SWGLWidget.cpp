
// SWOOZ
#include "SWGLWidget.h"
#include "interface/SWGLUtility.h"

// Std
#include <iostream>

// Qt
#include <QFileInfo>
#include <QTransform>

// MOC
#include "moc_SWGLWidget.cpp"


SWGLWidget::SWGLWidget(  QGLContext *oContext, QWidget* oParent) :
    QGLWidget( oContext, oParent ), m_glContext(oContext),  m_oTimer(new QBasicTimer)
{
    // miscellanous
        m_bVerbose = false;

	// init camera parameters
        m_bIsClickedMouse = false;
        m_bMidClick       = false;
        m_pCamera         = new SWQtCamera(QVector3D(0.f, 0.f, -0.15f), QVector3D(0.f, 0.f,  1.f), QVector3D(0.f, 1.f,  0.f));
	
	// init gl parameters
        if(m_bVerbose)
        {
            qDebug() << "OpenGl " << QGLWidget::format().majorVersion() << "," << QGLWidget::format().minorVersion();
        }

    // init point size
        m_glFSizePoint  = 2.0f;
	
	// init qt parameters
        setFocusPolicy( Qt::StrongFocus ); // set strong focus policy to be able to get the key events
}


SWGLWidget::~SWGLWidget()
{
    deleteAndNullify(m_oTimer);
    deleteAndNullify(m_pCamera);
}

void SWGLWidget::resetCamera(const QVector3D &oEyePosition, const QVector3D &oLookAt, const QVector3D &oUp)
{
    deleteAndNullify(m_pCamera);

    m_pCamera = new SWQtCamera(oEyePosition, oLookAt, oUp);
}

void SWGLWidget::initShaders( const QString& vertexShaderPath, const QString& fragmentShaderPath, QGLShaderProgram &oShader, cbool bBindShader)
{
	// load & compile vertex shader
    if (!oShader.addShaderFromSourceFile( QGLShader::Vertex, vertexShaderPath))
	{
        qWarning() << oShader.log();
	}

	// load & compile fragment shader
    if ( !oShader.addShaderFromSourceFile( QGLShader::Fragment, fragmentShaderPath ))
	{
        qWarning() << oShader.log();
	}

	// link shader pipeline
    if ( !oShader.link() )
	{
        qWarning() << "Could not link shader program : " << oShader.log();
	}
	
    if(bBindShader)
    {
        // bind shader pipeline
        if (!oShader.bind())
        {
            qWarning() << "Could not bind shader program : " << oShader.log();
        }
    }
}
    
void SWGLWidget::mousePressEvent(QMouseEvent *e)
{
	m_bIsClickedMouse = true;	   
    m_bMidClick       = (e->button() == Qt::MidButton);
}

void SWGLWidget::mouseReleaseEvent(QMouseEvent *e)
{
	m_bIsClickedMouse = false;
    m_bMidClick       = false;
}

void SWGLWidget::mouseMoveEvent(QMouseEvent *e)
{
	
    if(m_bIsClickedMouse)
	{
		QPoint l_oMiddle(m_oSize.width()/2, m_oSize.height()/2);
        QPoint l_oDiff = l_oMiddle - QPoint(e->x(), e->y());

        if(!m_bMidClick)
        {
            m_oCurrentRotation = QVector3D(-l_oDiff.y(), l_oDiff.x(), 0);
            m_pCamera->rotate(m_oCurrentRotation.x(), m_oCurrentRotation.y(), m_oCurrentRotation.z(), m_oCurrentRotation.length()/300);

        }
        else
        {
            if(e->x() < l_oMiddle.x()) // TODO : better camera
            {
                m_pCamera->moveLeft(0.01f);
            }
            else if(e->x() > l_oMiddle.x())
            {
                m_pCamera->moveRight(0.01f);
            }

            if(e->y() < l_oMiddle.y())
            {
                m_pCamera->moveUp(0.01f);
            }
            else if(e->y() > l_oMiddle.y())
            {
                m_pCamera->moveDown(0.01f);
            }
        }

        updateGL();
	}		
}

void SWGLWidget::wheelEvent(QWheelEvent *e)
{	   
    if(e->delta() > 0)
    {
        m_pCamera->moveForeward( e->delta()/120 * 0.02f);
    }
    else
    {
        m_pCamera->moveBackward(-e->delta()/120 * 0.02f);
    }

	updateGL();
}

void SWGLWidget::keyPressEvent(QKeyEvent *e)
{
	switch(e->key())
	{
		case Qt::Key_Plus:  // increase gl point size
			m_glFSizePoint < 10 ? m_glFSizePoint += 1 : m_glFSizePoint = 10;		
		break;
		case Qt::Key_Minus: // decrease gl point size
			m_glFSizePoint > 1  ? m_glFSizePoint -= 1 : m_glFSizePoint = 1;		
		break;	
		case Qt::Key_R:	    // reset camera view 
            m_pCamera->reset();
		break;									
		case Qt::Key_Left:
            m_pCamera->moveLeft(0.1f);
		break;
		case Qt::Key_Right:
            m_pCamera->moveRight(0.1f);
		break;
		case Qt::Key_Up:			
            m_pCamera->moveForeward(0.05f);
		break;
		case Qt::Key_Down:
            m_pCamera->moveBackward(0.05f);
		break;		
		case Qt::Key_Z:
            m_pCamera->rotateY(-3.f);
		break;		
		case Qt::Key_Q:
            m_pCamera->rotateX(3.f);
		break;		
		case Qt::Key_S:
            m_pCamera->rotateY(3.f);
		break;		
		case Qt::Key_D:
            m_pCamera->rotateX(-3.f);
		break;				
		
	}

	updateGL();
}

void SWGLWidget::timerEvent(QTimerEvent *e)
{
	// parameter "e" not used in the body of the function
	Q_UNUSED(e);
	
    if(m_bIsClickedMouse && !m_bMidClick)
    {
        m_pCamera->rotate(m_oCurrentRotation.x(), m_oCurrentRotation.y(), m_oCurrentRotation.z(), m_oCurrentRotation.length()/300);
    }

    updateGL();
}

void SWGLWidget::resizeGL( int i32Width, int i32Height )
{
	// set OpenGL viewport to cover whole widget
	glViewport(0, 0, i32Width, i32Height);

	m_oSize = QSize(i32Width, i32Height);
	
	// calculate aspect ratio
	qreal aspect = (qreal)i32Width / ((qreal)i32Height?i32Height:1);
	
	// set near plane to 3.0, far plane to 7.0, field of view 90 degrees
	// const qreal l_rZNear = 0.10, l_rZFar = 100.0, l_rFOV = 60.0;
    const qreal l_rZNear = 0.01, l_rZFar = 100.0, l_rFOV = 60.0;
	
	// reset projection
    m_oProjectionMatrix.setToIdentity();

	// set perspective projection
    m_oProjectionMatrix.perspective(l_rFOV, aspect, l_rZNear, l_rZFar);
}


void  SWGLWidget::drawAxes(QGLShaderProgram &oShader, QMatrix4x4 &mvpMatrix, cfloat fScale, const QVector3D &oOrigine)
{
    // bind shader
    if(!oShader.bind())
    {
        throw swExcept::swShaderGLError();
        return;
    }

    // set mode
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    QGLBuffer l_vertexBuffer(QGLBuffer::VertexBuffer);
    QGLBuffer l_indexBuffer(QGLBuffer::IndexBuffer);

    l_vertexBuffer.create();
    l_indexBuffer.create();

    float  *l_aFVertexBuffer   = new float[12];
    l_aFVertexBuffer[0] = oOrigine.x();
    l_aFVertexBuffer[1] = oOrigine.y();
    l_aFVertexBuffer[2] = oOrigine.z();

    l_aFVertexBuffer[3] = oOrigine.x();
    l_aFVertexBuffer[4] = oOrigine.y();
    l_aFVertexBuffer[5] = oOrigine.z() +1.f*fScale;

    l_aFVertexBuffer[6] = oOrigine.x();
    l_aFVertexBuffer[7] = oOrigine.y() +1.f*fScale;
    l_aFVertexBuffer[8] = oOrigine.z();

    l_aFVertexBuffer[9] = oOrigine.x() +1.f*fScale;
    l_aFVertexBuffer[10]= oOrigine.y();
    l_aFVertexBuffer[11]= oOrigine.z();

    uint32 *l_aUI32IndexBuffer = new uint[2];
    l_aUI32IndexBuffer[0] = 0;
    l_aUI32IndexBuffer[1] = 1;

    // allocate QGL buffers
    allocateBuffer(l_vertexBuffer, l_aFVertexBuffer, 4 *  3 * sizeof(float) );
    allocateBuffer(l_indexBuffer, l_aUI32IndexBuffer, 2 * sizeof(GLuint) );

    delete[] l_aFVertexBuffer;
    delete[] l_aUI32IndexBuffer;

    // set mvp matrix uniform value
    oShader.setUniformValue("mvpMatrix", mvpMatrix);

    // set color uniform value for the current line
    oShader.setUniformValue("uniColor", 1.f, 0.f, 0.f);
    drawBuffer(l_indexBuffer, l_vertexBuffer, oShader, GL_LINES);

    l_aUI32IndexBuffer = new uint[6];
    l_aUI32IndexBuffer[0] = 0;
    l_aUI32IndexBuffer[1] = 2;

    // allocate QGL buffers
    allocateBuffer(l_indexBuffer, l_aUI32IndexBuffer, 2 * sizeof(GLuint) );

    delete[] l_aUI32IndexBuffer;

    // set color uniform value for the current line
    oShader.setUniformValue("uniColor", 0.f, 1.f, 0.f);
    drawBuffer(l_indexBuffer, l_vertexBuffer, oShader, GL_LINES);

    l_aUI32IndexBuffer = new uint[6];
    l_aUI32IndexBuffer[0] = 0;
    l_aUI32IndexBuffer[1] = 3;

    // allocate QGL buffers
    allocateBuffer(l_indexBuffer, l_aUI32IndexBuffer, 2 * sizeof(GLuint) );

    delete[] l_aUI32IndexBuffer;

    // set color uniform value for the current line
    oShader.setUniformValue("uniColor", 0.f, 0.f, 1.f);
    drawBuffer(l_indexBuffer, l_vertexBuffer, oShader, GL_LINES);
}
