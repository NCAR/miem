###############
Getting Started
###############

Build and Install
=================

Requirements
------------

- A C++20-capable compiler (GCC 11+, Clang 14+, MSVC 19.30+)
- `CMake <https://cmake.org/>`_ 3.24 or later

MIEM is a header-only library. Its only test dependency,
`Google Test <https://github.com/google/googletest>`_, is fetched
automatically via CMake's ``FetchContent`` mechanism.

Building from Source
--------------------

.. code-block:: console

    $ git clone https://github.com/NCAR/miem.git
    $ cd miem
    $ mkdir build && cd build
    $ cmake ..
    $ make -j8

Because MIEM is a header-only library, the install step simply copies the
header files into the normal location required by your system:

.. code-block:: console

    $ make install

Running Tests
-------------

From the ``build/`` directory:

.. code-block:: console

    $ make test

Or with verbose output:

.. code-block:: console

    $ ctest --output-on-failure

CMake Options
=============

.. list-table::
   :header-rows: 1
   :widths: 30 10 60

   * - Option
     - Default
     - Description
   * - ``MIEM_ENABLE_TESTS``
     - ``ON``
     - Build the test suite
   * - ``MIEM_ENABLE_MEMCHECK``
     - ``OFF``
     - Enable Valgrind memory checking
   * - ``MIEM_ENABLE_COVERAGE``
     - ``OFF``
     - Enable code coverage output
   * - ``MIEM_BUILD_DOCS``
     - ``OFF``
     - Build Sphinx/Doxygen documentation

Docker
======

Build and run the MIEM container:

.. code-block:: console

    $ git clone https://github.com/NCAR/miem.git
    $ cd miem
    $ docker build -t miem -f docker/Dockerfile .
    $ docker run -it miem bash

Inside the container:

.. code-block:: console

    $ cd miem/build
    $ make test
