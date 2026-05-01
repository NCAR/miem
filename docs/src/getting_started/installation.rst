============
Installation
============

Requirements
============

- A C++20-capable compiler (GCC 11+, Clang 14+, MSVC 19.30+)
- CMake 3.24 or later

MIEM is a header-only library. Its only test dependency —
`Google Test <https://github.com/google/googletest>`_ — is fetched
automatically via CMake's ``FetchContent`` mechanism.

Building from Source
====================

.. code-block:: bash

   git clone https://github.com/NCAR/miem.git
   cd miem
   mkdir build && cd build
   cmake ..
   make -j8

Running Tests
-------------

From the ``build/`` directory:

.. code-block:: bash

   make test

Or with verbose output:

.. code-block:: bash

   ctest --output-on-failure

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

.. code-block:: bash

   git clone https://github.com/NCAR/miem.git
   cd miem
   docker build -t miem -f docker/Dockerfile .
   docker run -it miem bash

Inside the container:

.. code-block:: bash

   cd miem/build
   make test
