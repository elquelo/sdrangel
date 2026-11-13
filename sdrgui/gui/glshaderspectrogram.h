///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Copyright (C) 2016 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_GUI_GLSHADERSPECTROGRAM_H_
#define INCLUDE_GUI_GLSHADERSPECTROGRAM_H_

#include <QString>
#include <QOpenGLTexture>
#include <QOpenGLFunctions>

#include "export.h"
#include "dsp/spectrumsettings.h"

class QOpenGLShaderProgram;
class QMatrix4x4;
class QImage;

class SDRGUI_API GLShaderSpectrogram : protected QOpenGLFunctions
{
public:
    //! Texture rows not shown as grid rows: up to 2 rows of display lag (to interpolate between the
    //! last two rows) and 1 so that the oldest displayed row is never blended with the newest one
    static const int TimeMarginRows = 3;

    GLShaderSpectrogram();
    ~GLShaderSpectrogram();

    void initializeGL(int majorVersion, int minorVersion);
    void initColorMapTexture(const QString &colorMapName);
    void initTexture(const QImage& image);
    void subTexture(int xOffset, int yOffset, int width, int height, const void *pixels);
    //! Draw the surface. texturePosition is the (fractional) texture row that follows the most recent row
    //! displayed at the front edge. A fractional position makes the surface glide between whole rows.
    void drawSurface(SpectrumSettings::SpectrogramStyle style, const QMatrix4x4& vertexTransform, float texturePosition, bool invert);
    void cleanup();
    void translateX(float distance);
    void translateY(float distance);
    void translateZ(float distance);
    void rotateX(float degrees);
    void rotateY(float degrees);
    void rotateZ(float degrees);
    void setScaleX(float factor);
    void setScaleY(float factor);
    void setScaleZ(float factor);
    void userScaleZ(float factor);
    void reset();
    SpectrumSettings::Spectrogram3DView getView() const;
    void setView(const SpectrumSettings::Spectrogram3DView& view);
    void setAspectRatio(float aspectRatio);
    void verticalAngle(float degrees);
    void lightTranslateX(float distance);
    void lightTranslateY(float distance);
    void lightTranslateZ(float distance);
    void lightRotateX(float degrees);
    void lightRotateY(float degrees);
    void lightRotateZ(float degrees);
    void applyTransform(QMatrix4x4 &matrix);
    void applyPerspective(QMatrix4x4 &matrix);

private:
    void initColorMapTextureMutable(const QString &colorMapName);
    void initColorMapTextureImmutable(const QString &colorMapName);
    void initTextureImmutable(const QImage& image);
    void subTextureImmutable(int xOffset, int yOffset, int width, int height, const void *pixels);
    void initTextureMutable(const QImage& image);
    void subTextureMutable(int xOffset, int yOffset, int width, int height, const void *pixels);
    bool useImmutableStorage();
    void initGrid(int width, int height);
    void setPerspective();

    QOpenGLShaderProgram *m_programShaded;
    QOpenGLShaderProgram *m_programSimple;
    QOpenGLTexture *m_texture;
    unsigned int m_textureId;
    QOpenGLTexture *m_colorMapTexture;
    unsigned int m_colorMapTextureId;
    int m_textureWidth;
    int m_textureHeight;

    QOpenGLShaderProgram *m_programForLocs;     // Which program the locations are for
    int m_coord2dLoc;
    int m_textureTransformLoc;
    int m_vertexTransformLoc;
    int m_dataTextureLoc;
    int m_brightnessLoc;
    int m_colorMapLoc;
    int m_lightDirLoc;
    int m_lightPosLoc;

    bool m_useImmutableStorage;
    static const QString m_vertexShader2;
    static const QString m_vertexShader;
    static const QString m_geometryShader;
    static const QString m_fragmentShaderShaded;
    static const QString m_fragmentShaderSimple2;
    static const QString m_fragmentShaderSimple;

    QOpenGLVertexArrayObject *m_vao;
    QOpenGLBuffer *m_vertexBuf;
    QOpenGLBuffer *m_index0Buf;
    QOpenGLBuffer *m_index1Buf;

    float m_translateX;
    float m_translateY;
    float m_translateZ;
    float m_rotX;
    float m_rotY;
    float m_rotZ;
    float m_scaleX;
    float m_scaleY;
    float m_scaleZ;
    float m_userScaleZ;
    float m_verticalAngle;
    float m_aspectRatio;
    float m_lightTranslateX;
    float m_lightTranslateY;
    float m_lightTranslateZ;
    float m_lightRotX;
    float m_lightRotY;
    float m_lightRotZ;
    QMatrix4x4 m_perspective;
    int m_gridWidthElements;
    int m_gridHeightElements;
    int m_gridLineIndexCount;
    int m_gridTriangleIndexCount;

};

#endif /* INCLUDE_GUI_GLSHADERSPECTROGRAM_H_ */
