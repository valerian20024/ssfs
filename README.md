# SSFS (Simple and secure filesystem)

This project is held by Valérian Wislez in the scope of the course *2024-2025-INFO0940-A-a* **Operating systems 30h Th, 6h Pr, 30h Proj.**

It also serves as an exercise to practice better C, with a focus on error management, code structure and documentation.

## Error management

`goto <label>` statements have been used to force the flow to some chokepoints where the error management is perfomed by e.g. cleaning, freeing, reverting actions done in the function.
While it's not a recommended practice, it seemed to be the perfect solution to balance readability, conciseness and simplicity. Different error codes represent the behavior of the program and so there is a need to be able to forward those error codes as well in the process. 

## Code structure

A not so trivial Makefile (thanks to [Makefile Tutorials](https://makefiletutorial.com/)!) has been written to allow a nice, clear and extensive structure for files, including an `obj/` directory in which all object files are dumped when compiling. The Makefile had to comply with a given code structure in the statement (`vdisk/`, `include/`). It also outputs a simple colored notification of the ongoing compilation process.

## Documentation

An effort has been made to give a good Doxygen documentation for all functions in the program, as well as a documentation header for each file.