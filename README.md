# SSFS (Simple and Secure FileSystem)

[![HitCount](https://hits.dwyl.com/valerian20024/ssfs.svg?style=flat-square)](http://hits.dwyl.com/valerian20024/ssfs)


This project is held by Valérian Wislez in the scope of the course *2024-2025-INFO0940-A-a* **Operating systems 30h Th, 6h Pr, 30h Proj.** and beyond ... *(hopefully some day I'll make a more efficient version of SSFS part of my own OS)*

It also serves as an exercise to practice better C, with a focus on error management, code structure and documentation.

## Error management

`goto <label>` statements have been used to force the flow to some chokepoints where the error management is perfomed by e.g. cleaning, freeing, reverting actions done in the function.
While it's not a recommended practice, it seemed to be the perfect solution to balance readability, conciseness and simplicity. Different error codes represent the behavior of the program and so there is a need to be able to forward those error codes as well in the process. 

## Code structure

A not so trivial Makefile (thanks to [Makefile Tutorials](https://makefiletutorial.com/)!) has been written to allow a nice, clear and extensive structure for files, including an `obj/` directory in which all object files are dumped when compiling. The Makefile had to comply with a given code structure in the statement (`vdisk/`, `include/`). It also outputs a simple colored notification of the ongoing operations.

## Documentation

An effort has been made to give a good Doxygen documentation for all functions in the program, as well as a documentation header for each file.

# Make commands

`make all` or simply `make` will compile all the files into the `obj/` directory. Then it will link them and the required libraries into the final executable `fs_test`. The program requires the [libbsd-dev](https://packages.debian.org/sid/libbsd-dev) library to perform some strings operations. As such, this command will also call the `check-libs` which checks the presence of these libraries and installs them if necessary.

`make clean` will remove all the byproducts of compilation, the executable, the source code archive, etc.

`make build` will build a `src.tar.gz` that contains all the current source code in the `src/` directory.

`make show` will transform the output files from their raw hexadecimal representation into an actual readable file. However, you might still to change the file extension format to be able to open it easily. The C program will output hexadecimal into `output/some_file.hex`. If `output/` doesn't exist, run `make all` again. This command requires to have the [xxd](https://linux.die.net/man/1/xxd)  program installed for reversing hexadecimal.

`make debug` is simply there for development purposes, it outputs more verbose informations. It's precise operations might change in the future.

# Ideas for future smarter implementation

1. Don't take 32 bits to represent a boolean ...

2. Smart writing system mode :
    For a file of size 0 :
    - case 0 : len < 0 : error
    - case 1 : len = 0 : trivially OK
    - case 2 : offset < 0 and offset + len < 0 : error
    - case 3 : offset < 0 and offset + len >= 0 : OK (as the user wants to write over the file and beyond, this special case with a negative offset can be treated if we shift all writing operations to start from 0 to len + (-offset))
    case 4 : offset >= 0 : OK

    For a file of size > 0 :
    - case 0 : len < 0 : error
    - case 1 : len = 0 : trivially OK
    - case 2 : offset < 0 and offset + len <= 0 : error
    - case 3 : offset < 0 : error
    - case 4 : offset >= 0 and offset + len <= size - 1 (must recheck bounds)
    - case 5 : offset < 0 and offst + len >= size : OK (as the user wants to write over the all file and beyond, this special case with a negative offset can be treated if we shift all writing operations to start from 0 to len + (-offset))
    - case 6 : offset >= 0 and offset + len >= size : OK, simply cut into write in file and write out file.
    - case 7 : offset >= size : OK, simply write_out_file and fill the gap with zeros if offset > size