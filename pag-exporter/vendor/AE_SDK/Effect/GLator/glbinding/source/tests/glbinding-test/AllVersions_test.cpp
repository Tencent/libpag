#include <gmock/gmock.h>

#include <glbinding/gl/gl.h>
#include <glbinding/gl10/gl.h>
#include <glbinding/gl10ext/gl.h>
#include <glbinding/gl11/gl.h>
#include <glbinding/gl11ext/gl.h>
#include <glbinding/gl12/gl.h>
#include <glbinding/gl12ext/gl.h>
#include <glbinding/gl13/gl.h>
#include <glbinding/gl13ext/gl.h>
#include <glbinding/gl14/gl.h>
#include <glbinding/gl14ext/gl.h>
#include <glbinding/gl15/gl.h>
#include <glbinding/gl15ext/gl.h>
#include <glbinding/gl20/gl.h>
#include <glbinding/gl20ext/gl.h>
#include <glbinding/gl21/gl.h>
#include <glbinding/gl21ext/gl.h>
#include <glbinding/gl30/gl.h>
#include <glbinding/gl30ext/gl.h>
#include <glbinding/gl31/gl.h>
#include <glbinding/gl31ext/gl.h>
#include <glbinding/gl32/gl.h>
#include <glbinding/gl32core/gl.h>
#include <glbinding/gl32ext/gl.h>
#include <glbinding/gl33/gl.h>
#include <glbinding/gl33core/gl.h>
#include <glbinding/gl33ext/gl.h>
#include <glbinding/gl40/gl.h>
#include <glbinding/gl40core/gl.h>
#include <glbinding/gl40ext/gl.h>
#include <glbinding/gl41/gl.h>
#include <glbinding/gl41core/gl.h>
#include <glbinding/gl41ext/gl.h>
#include <glbinding/gl42/gl.h>
#include <glbinding/gl42core/gl.h>
#include <glbinding/gl42ext/gl.h>
#include <glbinding/gl43/gl.h>
#include <glbinding/gl43core/gl.h>
#include <glbinding/gl43ext/gl.h>
#include <glbinding/gl44/gl.h>
#include <glbinding/gl44core/gl.h>
#include <glbinding/gl44ext/gl.h>
#include <glbinding/gl45/gl.h>
#include <glbinding/gl45core/gl.h>
#include <glbinding/gl45ext/gl.h>

TEST(AllVersions, Compilation)
{
    SUCCEED();  // compiling this file without errors and warnings results in successful test
}
