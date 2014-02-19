
/**
 * \file SWGLMeshWidget.h
 * \brief Defines SWGLMeshWidget
 * \author Florian Lance
 * \date 11/07/13
 */

#ifndef _SWGLMESHWIDGET_
#define _SWGLMESHWIDGET_

#include "mesh/SWMesh.h"

#include "SWGLWidget.h"

class SWGLMeshWidget : public SWGLWidget
{
    Q_OBJECT

    public:

        /**
         * \brief constructor of SWGLMeshWidget
         * \param [in] context              : ...
         * \param [in] parent               : ...
         * \param [in] sVertexShaderPath    : ...
         * \param [in] sFragmentShaderPath  : ...
         */
        SWGLMeshWidget(QGLContext *context, QWidget* parent = 0, const QString &sVertexShaderPath = "", const QString &sFragmentShaderPath = "");

        /**
         * \brief destructor of SWGLMeshWidget
         */
        ~SWGLMeshWidget();

    protected:

        /**
         * \brief Initialize the OpenGl scene
         */
        virtual void initializeGL();

        /**
         * \brief Draw the OpenGl scene
         */
        virtual void paintGL();

        /**
         * \brief Init buffers.
         */
        void initMeshBuffers();


    public slots:

        /**
         * \brief Draw the mesh.
         */
        void drawMesh();  

        /**
         * @brief drawMeshLines
         */
//        void drawMeshLines();

        /**
         * \brief Set the mesh to draw
         * \param [in] oMesh    : mesh to draw
         */
        void setMesh(swMesh::SWMesh &oMesh);

        /**
         * \brief Set the mesh to draw
         * \param [in] pMesh    : pointer to the mesh to draw
         */
        void setMesh(swMesh::SWMesh *pMesh);

        /**
         * \brief Set the texture for the mesh.
         * \param [in] sTexturePath : path of the texture.
         */
        void setTexture(const QString &sTexturePath);

        /**
         * @brief setTexture
         * @param oTexture
         */
        void setTexture(const QImage &oTexture);

        /**
         * @brief applyTexture
         * @param bApplyTexture
         */
        void applyTexture(const bool bApplyTexture);

        /**
         * @brief SWGLMeshWidget::setMeshLinesRender
         * @param bRenderLines
         */
        void setMeshLinesRender(const bool bRenderLines = true);

    private :

        bool m_bInitCamWithCloudPosition;

        QString m_sVertexShaderPath;    /**< ... */
        QString m_sFragmentShaderPath;  /**< ... */

        bool m_bLinesRender;                  /**< ... */
        bool m_bApplyTexture;                  /**< ... */

        QGLShaderProgram m_oShaderMesh;         /**< ... */
        QGLShaderProgram m_oShaderLines;    /**< ... */

        QImage m_oTexture;

        QGLBuffer m_vertexBuffer;   /**< ... */
        QGLBuffer m_indexBuffer;    /**< ... */
        QGLBuffer m_normalBuffer;   /**< ... */
        QGLBuffer m_textureBuffer;  /**< ... */

        QReadWriteLock m_oParamMutex; /**< ... */

        GLuint m_textureLocation;

        QMatrix4x4  m_oMVPMatrix;	/**< ... */

        swMesh::SWMesh *m_pMesh;            /**< ... */
};

#endif

