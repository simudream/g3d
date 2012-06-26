/**
 \file GLG3D/Shader2.cpp
  
 \maintainer Morgan McGuire, Michael Mara http://graphics.cs.williams.edu
 
 \created 2012-06-13
 \edited 2012-06-13

 TODO: Add parameter to turn off preprocessor to Specification



 */

#include "G3D/platform.h"
#include "GLG3D/Shader2.h"
#include "GLG3D/GLCaps.h"
#include "G3D/fileutils.h"
#include "G3D/Log.h"
namespace G3D {

Shader2::FailureBehavior Shader2::s_failureBehavior = Shader2::PROMPT;



Shader2::Shader2(Specification s){
    m_specification = s;
    m_shaderProgram = ShaderProgram::create(s);
}



Shader2::Specification::Specification(const std::string& f0, const std::string& f1, 
        const std::string& f2, const std::string& f3, const std::string& f4) {
    const Array<std::string> filenames(f0, f1, f2, f3, f4);
    for(int i = 0; i < filenames.size(); ++i){
        const std::string fname = filenames[i];
        if(fname != ""){ // Skip blanks
            alwaysAssertM(fname.size() > 4, format("Invalid filename given to Shader2::Specification():\n%s", fname.c_str()));

            size_t extensionStart = fname.find_last_of('.');

            alwaysAssertM(extensionStart != std::string::npos && // Has a period
                (fname.size() - extensionStart) == 4,  // Extension is 3 characters

                format("Invalid filename given to Shader2::Specification():\n%s", fname.c_str()));
            const std::string extension = fname.substr(extensionStart + 1, 3);

            // Determine the stage
            ShaderStage stage;
            if(extension == "vrt" || extension == "vtx"){
                stage = VERTEX;
            } else if (extension == "ctl" || extension == "hul") {
                stage = TESSELLATION_CONTROL;
            } else if (extension == "evl" || extension == "dom") {
                stage = TESSELLATION_EVAL;
            } else if (extension == "geo") {
                stage = GEOMETRY;
            } else if (extension == "pix" || extension == "frg") {
                stage = PIXEL;
            } else {
                alwaysAssertM(true, format("Invalid filename given to Shader2::Specification():\n%s", fname.c_str()));
            }

            // Finally, set the source
            shaderStage[stage] = Source(FILE, fname);
            
        }
    }

}

Shader2::Ref Shader2::fromFiles(
        const std::string& f0, 
        const std::string& f1, 
        const std::string& f2, 
        const std::string& f3, 
        const std::string& f4){
    Specification s = Specification(f0, f1, f2, f3, f4);
    return create(s);
}





Shader2::Specification::Specification(){}

Shader2::Specification::Specification(const Any& any){
    /* if(any.containsKey("vertexFile")){
        vertex.val  = any["vertexFile"].string();
        vertex.type = FILE;
    } else if(any.containsKey("vertexString")){
        vertex.val = any["vertexString"].string();
    }
    if(any.containsKey("tessellationFile")){
        tessellation.val  = any["tessellationFile"].string();
        tessellation.type = FILE;
    } else if(any.containsKey("tessellationString")){
        tessellation.val = any["tessellationString"].string();
    }
    if(any.containsKey("tessellationControlFile")){
        tessellationControl.val  = any["tessellationControlFile"].string();
        tessellationControl.type = FILE;
    } else if(any.containsKey("tessellationControlString")){
        tessellationControl.val = any["tessellationControlString"].string();
    } 
    if(any.containsKey("geometryFile")){
        geometry.val  = any["geometryFile"].string();
        geometry.type = FILE;
    } else if(any.containsKey("geometryString")){
        geometry.val = any["geometryString"].string();
    } 
    if(any.containsKey("pixelFile")){
        pixel.val  = any["pixelFile"].string();
        pixel.type = FILE;
    } else if(any.containsKey("pixelString")){
        pixel.val = any["pixelString"].string();
    } */
}



static void readAndAppendShaderLog(const char* glInfoLog, std::string& messages, const std::string& name){
    int c = 0;
    // Copy the result to the output string, prepending the filename
    while (glInfoLog[c] != '\0') {
        messages += name;

        // Copy until the next newline or end of string
        std::string line;
        while (glInfoLog[c] != '\n' && glInfoLog[c] != '\r' && glInfoLog[c] != '\0') {
            line += glInfoLog[c];
            ++c;
        }

        if (beginsWith(line, "ERROR: ")) {
            // NVIDIA likes to preface messages with "ERROR: "; strip it off
            line = line.substr(7);

            if (beginsWith(line, "0:")) {
                // Now skip over the line number and wrap it in parentheses.
                line = line.substr(2);
                size_t i = line.find(':');
                if (i != std::string::npos) {
                    // Wrap the line number in parentheses
                    line = "(" + line.substr(0, i) + ")" + line.substr(i);
                } else {
                    // There was no line number, so just add a colon
                    line = ": " + line;
                }
            } else {
                line = ": " + line;
            }
        }

        messages += line;
        
        if (glInfoLog[c] == '\r' && glInfoLog[c + 1] == '\n') {
            // Windows newline
            messages += NEWLINE;
            c += 2;
        } else if (glInfoLog[c] == '\r' && glInfoLog[c + 1] != '\n') {
            // Dangling \r; treat it as a newline
            messages += "\r\n";
            ++c;
        } else if (glInfoLog[c] == '\n') {
            // Newline
            messages += NEWLINE;
            ++c;
        }
    }
    
}



std::string stageName(int s){
    switch(s){

    case Shader2::VERTEX:
        return "Vertex";
    case Shader2::TESSELLATION_CONTROL:
        return "Tesselation Control";
    case Shader2::TESSELLATION_EVAL:
        return "Tesselation Evaluation";
    case Shader2::GEOMETRY:
        return "Geometry";
    case Shader2::PIXEL:
        return "Pixel";
    default:
        return "Invalid Stage";
    }
}


GLenum glShaderType(int s){
    switch(s){

    case Shader2::VERTEX:
        return GL_VERTEX_SHADER;
    case Shader2::TESSELLATION_CONTROL:
        //return GL_TESS_CONTROL_SHADER;
    case Shader2::TESSELLATION_EVAL:
        //return GL_TESS_EVALUATION_SHADER;
    case Shader2::GEOMETRY:
        return GL_GEOMETRY_SHADER;
    case Shader2::PIXEL:
        return GL_FRAGMENT_SHADER;
    default:
        return -1;
    }
}

Shader2::ShaderProgram::Ref Shader2::ShaderProgram::create(Shader2::Specification spec){
    return new ShaderProgram(spec);
}

Shader2::ShaderProgram::ShaderProgram(Shader2::Specification spec){
    m_ok = true;
    debugAssertGLOk();
    if (! GLCaps::supports_GL_ARB_shader_objects()) {
        m_messages = "This graphics card does not support GL_ARB_shader_objects.";
        m_ok = false;
        return;
    }
    debugAssertGLOk();
    bool isEmptyStage[STAGE_COUNT];
    for(int s = 0; s < STAGE_COUNT; ++s){
        debugAssertGLOk();
        // Read the code into a string
        const Source& source = spec.shaderStage[s];
        std::string code;
        std::string name;
        if(source.type == STRING){
            code = source.val;
            name = "'" + stageName(s) + " from string'";
        } else {
            // TODO: catch exception?
            isEmptyStage[s] = (source.val == "");
            if(!isEmptyStage[s]){
                code = readWholeFile(source.val);
                
            } else {
                code = "";
            }
            name = source.val;
        }
        debugAssertGLOk();
        // If it's empty, there's nothing to compile!
        isEmptyStage[s] = (code == "");
        if(!isEmptyStage[s]){
            debugAssertGLOk();
            GLint compiled = GL_FALSE;
            
            GLhandleARB& glShaderObject = m_glShaderObject[s];
            debugAssertGLOk();
            glShaderObject = glCreateShader(glShaderType(s));
            debugAssertGLOk();
            // Compile the shader
            GLint length = (GLint)code.length();
            const GLchar* codePtr = static_cast<const GLchar*>(code.c_str());
            debugAssertGLOk();
            // Show the preprocessed code: TODO remove, this is just for testing
            fprintf(stderr, "\"%s\"\n", code.c_str());
            debugAssertGLOk();
            glShaderSource(glShaderObject, 1, &codePtr, &length);
            debugAssertGLOk();
            glCompileShader(glShaderObject);
            debugAssertGLOk();
            glGetShaderiv(glShaderObject, GL_COMPILE_STATUS, &compiled);

            // Read the result of compilation
            GLint maxLength;
            debugAssertGLOk();
            glGetShaderiv(glShaderObject, GL_INFO_LOG_LENGTH, &maxLength);
            debugAssertGLOk();
            GLchar* pInfoLog = (GLchar *)malloc(maxLength * sizeof(GLchar));
            glGetShaderInfoLog(glShaderObject, maxLength, &length, pInfoLog);
            debugAssertGLOk();
    
            readAndAppendShaderLog(pInfoLog, m_messages, name);
            debugAssertGLOk();
            free(pInfoLog);
            m_ok = (compiled == GL_TRUE);
        }
    }
    debugAssertGLOk();
    m_glProgramObject = glCreateProgram();
    debugAssertGLOk();
    // Attach
    for(int s = 0; s < STAGE_COUNT; ++s){
        debugAssertGLOk();
        if(!isEmptyStage[s]){
            glAttachShader(m_glProgramObject, m_glShaderObject[s]);
        }
        debugAssertGLOk();
    }

    // Link
    debugAssertGLOk();
    glLinkProgram(m_glProgramObject);
    debugAssertGLOk();

    // Read back messages
    GLint linked;
    glGetObjectParameterivARB(m_glProgramObject, GL_OBJECT_LINK_STATUS_ARB, &linked);
    GLint maxLength = 0, length = 0;
    glGetObjectParameterivARB(m_glProgramObject, GL_OBJECT_INFO_LOG_LENGTH_ARB, &maxLength);
    GLcharARB* pInfoLog = (GLcharARB *)malloc(maxLength * sizeof(GLcharARB));
    glGetInfoLogARB(m_glProgramObject, maxLength, &length, pInfoLog);

    m_messages += std::string("Linking\n") + std::string(pInfoLog) + "\n";
    logPrintf("%s\n", m_messages.c_str());
}


Shader2::Source::Source(const std::string& value){
    // All valid shader code has a semicolon, no filenames do if you're sane.
    alwaysAssertM(value.find(';') != std::string::npos, format("The Source(string) constructor only \
        accepts GLSL code, not filenames. The passed in string was:\n %s \nIf this looks like code, look \
        for missing semicolons (all valid code should have at least one).\nIf this is a filename, use \
        the Source(SourceType, string) constructor instead, with a SourceType of FILE.\n", value.c_str()));
    type = STRING;
    val = value;
}

Shader2::Source::Source(SourceType t, const std::string& value) :
    type(t), val(value){}

Shader2::Source::Source(){

}

void Shader2::setFailureBehavior(FailureBehavior f){
    s_failureBehavior = f;
}
    
void Shader2::reload(){
       
}

bool Shader2::ok() const{
    return m_ok;
}

Shader2::Ref Shader2::create(const Specification& s){
    return new Shader2(s);
}

    



}
