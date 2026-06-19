//#pragma once
//#include <GL/glew.h>
#include <glfw/glfw3.h>
//
//#include "Core.h"
//#include <windows.h>
//
////unsigned int ApplicationWindow::mWindowWidth;
////unsigned int ApplicationWindow::mWindowHeight;
////
////
////void QuickErrorCallback(int error, const char* desc)
////{
////	fprintf(stderr, "GLFW ERROR %d: %s\n", error, desc);
////}
////
////
////
//////void QuickDebugCallback(GLenum src, GLenum type,
//////	GLuint id, GLenum severity,
//////	GLsizei length, const GLchar* msg,
//////	const void* user_param)
//////{
//////	if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
//////		return;
//////	printf("OpenGL DEBUG: %s\n", msg);
//////}
////
//////---------------------------------------------------------------------
////#define ANSI_RED_COLOUR "\033[1;31m"	   //4
////#define ANSI_GREEN_COLOUR "\033[1;32m"	   //2
////#define ANSI_YELLOW_COLOUR "\033[1;33m"	   //3
////#define ANSI_BLUE_COLOUR "\033[1;34m"      //1
////#define ANSI_RESET_COLOUR "\033[0m"
////
////
////enum class ESeverityLvl : uint8_t
////{
////	High = 3,
////	Meduim = 2,
////	Low = 1,
////	Notification = 0
////};
////
////
////constexpr const char* GL_SeverityEnumToText(GLenum severity)
////{
////	switch (severity)
////	{
////	case GL_DEBUG_SEVERITY_NOTIFICATION: return "GL_DEBUG_NOTIFICATION";
////	case GL_DEBUG_SEVERITY_MEDIUM: return "GL_DEBUG_WARNING";
////	case GL_DEBUG_SEVERITY_LOW: return "GL_DEBUG_MINOR_ISSUE";
////	case GL_DEBUG_SEVERITY_HIGH: return "GL_DEBUG_ERROR";
////	default: return "GL_DEBUG_UNKNOWN";
////	}
////}
////
////constexpr const char* GL_TypeEnumToText(GLenum type)
////{
////	switch (type)
////	{
////	case GL_DEBUG_TYPE_ERROR: return "API or shader error";
////	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "Using deprecated features";
////	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: return "Undefined behaviour";
////	case GL_DEBUG_TYPE_PORTABILITY: return "Non-portable use (vendor-specific feature)";
////	case GL_DEBUG_TYPE_PERFORMANCE: return "Performance warning";
////	case GL_DEBUG_TYPE_OTHER: return "Misc driver message";
////	case GL_DEBUG_TYPE_MARKER: return "Debug marker by application";
////	case GL_DEBUG_TYPE_PUSH_GROUP: return "Group push event";
////	case GL_DEBUG_TYPE_POP_GROUP: return "Group pop event";
////	default: return "Unknown";
////	}
////}
////
////
////constexpr const char* GL_SourceEnumToText(GLenum src)
////{
////	switch (src)
////	{
////	case GL_DEBUG_SOURCE_API: return "OpenGL API";
////	case GL_DEBUG_SOURCE_WINDOW_SYSTEM: return "Window system";
////	case GL_DEBUG_SOURCE_SHADER_COMPILER: return "shader compiler";
////	case GL_DEBUG_SOURCE_THIRD_PARTY: return "Third party/driver";
////	case GL_DEBUG_SOURCE_APPLICATION: return "Custom Application";
////	case GL_DEBUG_SOURCE_OTHER: return "Misc driver source";
////	default: return "Unknown";
////	}
////}
////
////
////inline constexpr const char* ANSI_GL_DebugCol(GLenum severity)
////{
////	switch (severity)
////	{
////	case GL_DEBUG_SEVERITY_NOTIFICATION: return ANSI_BLUE_COLOUR;
////	case GL_DEBUG_SEVERITY_LOW: return ANSI_GREEN_COLOUR;
////	case GL_DEBUG_SEVERITY_MEDIUM: return ANSI_YELLOW_COLOUR;
////	case GL_DEBUG_SEVERITY_HIGH: return ANSI_RED_COLOUR;
////	default: return ANSI_GREEN_COLOUR;
////	}
////}
//
//
//class Window
//{
//public:
//	bool Init(int width, int height, bool v_sync, bool full_screen =false)
//	{
//		//glfwSetErrorCallback(QuickErrorCallback);
//
//		if (!glfwInit())
//			//if (!glfw_state)
//		{
//			LOG_MSG("Failed to initialise GLFW!!!!!!\n");
//			return false;
//		}
//
//		mWindowWidth = width;
//		mWindowHeight =height;
//
//		///create back window
//		if (full_screen)
//		{
//			mWindowWidth = GetSystemMetrics(SM_CXSCREEN);
//			mWindowHeight = GetSystemMetrics(SM_CYSCREEN);
//		}
//
//		mWindow = glfwCreateWindow(mWindowWidth, mWindowHeight, "Multicore", nullptr, nullptr);
//
//		const char* err_desc;
//		if (!mWindow)
//		{
//			glfwGetError(&err_desc);
//			LOG_MSG("Failed to create GLFW Window: " << err_desc << "\n");
//			glfwTerminate();
//			return false;
//		}
//
//		glfwMakeContextCurrent(mWindow);
//
//		glfwSwapInterval(int(v_sync));
//
//		glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
//
//
//
//		GLenum GlewInitResult = glewInit();
//		if (GlewInitResult != GLEW_OK)
//		{
//			LOG_MSG("Glew Init failed, ERROR: " << glewGetErrorString(GlewInitResult) << "\n");
//			glfwDestroyWindow(mWindow);
//			glfwTerminate();
//			return false;
//		}
//
//		glfwSetWindowUserPointer(mWindow, this);
//		glfwSetKeyCallback(mWindow, KeyboardInputCallback);
//
//		//glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
//		////}
//		//glEnable(GL_DEBUG_OUTPUT);
//		//glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
//		//glDebugMessageCallback(ErrorMessageCallback, nullptr);
//
//		return true;
//	}
//
//
//	void SwapBuffer() const
//	{
//		glfwSwapBuffers(mWindow);
//	}
//
//
//	void FlushAndSwapBuffer()
//	{
//		glfwSwapBuffers(mWindow);
//		glfwPollEvents();
//	}
//
//	bool IsActive()
//	{
//		return !glfwWindowShouldClose(mWindow);
//	}
//	void SetShouldClose()
//	{
//		glfwSetWindowShouldClose(mWindow, true);
//	}
//
//	void Close() const
//	{
//		glfwDestroyWindow(mWindow);
//		glfwTerminate();
//	}
//
//	void LockCursor(bool v)
//	{
//		glfwSetInputMode(mWindow, GLFW_CURSOR, (v) ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
//	}
//	void ChangeWindowTitle(const char* name)
//	{
//		glfwSetWindowTitle(mWindow, name);
//	}
//	void const SetVSync(bool v)
//	{
//		glfwSwapInterval(int(v));
//	}
//
//
//	using KeyboardFuncCB = std::function<void(int key, int scancode, int action, int mods)>;
//
//	static void KeyboardInputCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
//	{
//		Window* user = (Window*)(glfwGetWindowUserPointer(window));
//		(user && user->mUserKeyboardFunc) ? user->mUserKeyboardFunc(key, scancode, action, mods) : void(0);
//	}
//	void SetKeyboardCallback(KeyboardFuncCB func)
//	{
//		mUserKeyboardFunc = func;
//	}
//
//
//private:
//	float mWindowWidth ;
//	float mWindowHeight;
//
//	GLFWwindow* mWindow;
//
//	KeyboardFuncCB mUserKeyboardFunc;
//};
//
