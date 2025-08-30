#pragma once

#include "Application.h" // We need the full definition of Application

#ifdef WL_PLATFORM_WINDOWS

namespace AlgeUI {

	/**
	 * @brief Forward declaration of the client-implemented function.
	 *
	 * This tells the AlgeUI library that the user of the library (the client application)
	 * will provide the implementation for this function. It's the main contract.
	 */
	extern Application* CreateApplication(int argc, char** argv);

	/**
	 * @brief The main entry point for the AlgeUI engine.
	 *
	 * This function initializes the application, runs its main loop, and then
	 * cleans up. It can loop to allow for application restarts.
	 */
	int Main(int argc, char** argv)
	{
		bool applicationRunning = true;
		while (applicationRunning)
		{
			AlgeUI::Application* app = AlgeUI::CreateApplication(argc, argv);
			app->Run(); // The Run() method will now block until the app is closed.
			delete app;

			// In a more advanced setup, the app could return a status
			// to signal if a restart is needed. For now, we exit the loop.
			applicationRunning = false;
		}

		return 0;
	}

}

#ifdef WL_DIST

#include <Windows.h>

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
	// In distribution builds, we use the WinMain entry point for a windowed application.
	return AlgeUI::Main(__argc, __argv);
}

#else

int main(int argc, char** argv)
{
	// In debug and release builds, we use the standard 'main' entry point for a console application.
	return AlgeUI::Main(argc, argv);
}

#endif // WL_DIST

#endif // WL_PLATFORM_WINDOWS