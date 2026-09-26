/* OpenGL 4.6 entry points, resolved by name at startup (one string, one table). */
#define GLFN(X) \
 X(GLuint,  glCreateShader, (GLenum)) \
 X(void,    glShaderSource, (GLuint, GLsizei, const GLchar *const *, const GLint *)) \
 X(void,    glCompileShader, (GLuint)) \
 X(GLuint,  glCreateProgram, (void)) \
 X(void,    glAttachShader, (GLuint, GLuint)) \
 X(void,    glLinkProgram, (GLuint)) \
 X(void,    glUseProgram, (GLuint)) \
 X(void,    glUniform1fv, (GLint, GLsizei, const GLfloat *)) \
 X(void,    glUniform4fv, (GLint, GLsizei, const GLfloat *)) \
 X(void,    glCreateTextures, (GLenum, GLsizei, GLuint *)) \
 X(void,    glTextureStorage2D, (GLuint, GLsizei, GLenum, GLsizei, GLsizei)) \
 X(void,    glTextureSubImage2D, (GLuint, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *)) \
 X(void,    glTextureParameteri, (GLuint, GLenum, GLint)) \
 X(void,    glBindTextureUnit, (GLuint, GLuint)) \
 X(void,    glCreateFramebuffers, (GLsizei, GLuint *)) \
 X(void,    glNamedFramebufferTexture, (GLuint, GLenum, GLuint, GLint)) \
 X(void,    glNamedFramebufferDrawBuffers, (GLuint, GLsizei, const GLenum *)) \
 X(void,    glBindFramebuffer, (GLenum, GLuint)) \
 X(void,    glCreateVertexArrays, (GLsizei, GLuint *)) \
 X(void,    glBindVertexArray, (GLuint)) \
 X(void,    glGenerateTextureMipmap, (GLuint)) \
 X(void,    glBlitNamedFramebuffer, (GLuint, GLuint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum)) \
 X(void,    glGetShaderInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *)) \
 X(void,    glGetProgramInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *)) \
 X(void,    glGetShaderiv, (GLuint, GLenum, GLint *)) \
 X(void,    glGetProgramiv, (GLuint, GLenum, GLint *))

#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif
#define GLDECL(r, n, a) typedef r (APIENTRYP PFN_##n) a; static PFN_##n n;
GLFN(GLDECL)
#define GLNAME(r, n, a) #n "\0"
static const char gl_names[] = GLFN(GLNAME);
#define GLPTR(r, n, a) (void **)&n,
static void **const gl_ptrs[] = { GLFN(GLPTR) };

static void gl_load(void *(*get)(const char *))
{
    const char *s = gl_names;
    unsigned i;
    for (i = 0; i < sizeof(gl_ptrs) / sizeof(gl_ptrs[0]); i++) {
        *gl_ptrs[i] = get(s);
        while (*s++) ;
    }
}
