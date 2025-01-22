
# Introduction to SDL with Visual Studio

The easiest way to use SDL is to include it as a subproject in your project.

We'll start by creating a simple project to build and run [hello.c](hello.c)

- Create a new project in Visual Studio, using the C++ Empty Project template
- Add hello.c to the Source Files
- Right click the solution, select add an existing project, navigate to VisualC/SDL and add SDL.vcxproj
- Select your main project and go to Project -> Add Reference and select SDL3
- Select your main project and go to Project -> Properties, set the filter at the top to "All Configurations" and "All Platforms", select VC++ Directories and add the SDL include directory to "Include Directories"
- Build and run!

