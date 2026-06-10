.. _Contributing:

Contributing
============

For all proposed changes (bug fixes, new features, documentation updates, etc.)
please file an `issue <https://github.com/NCAR/miem/issues/new/choose>`_
detailing what your ask is or what you intend to do.

The NSF NCAR software developers will work with you on that issue to help
recommend implementations, work through questions, or provide other background
knowledge.

Testing
-------

Any code that you contribute must include associated unit tests. Our tests run
across various platforms and with different configurations automatically. By
including tests that exercise your code, you help to minimize the amount of
time debugging platform-portability issues. New features must also include at
least an example in our documentation.

Style guide
-----------

We (mostly) follow the
`Google C++ style guide <https://google.github.io/styleguide/cppguide.html>`_.
Please attempt to do the same to minimize comments on PRs. We are not dogmatic,
however, and reasonable exceptions are allowed, especially where they help to
simplify code or API interfaces.

We run ``clang-format`` over the codebase, so don't worry too much about
formatting your code by hand. The use of ``clang-format`` enforces a
consistency that means moving from file to file shouldn't have a large
cognitive load on the developer.

Building the documentation
--------------------------

All of our docs are stored in the ``docs`` directory. There are several Python
dependencies that need to be installed. To make this easy, we provide a
``requirements.txt`` file that you can use.

.. code-block:: console

    $ python -m venv miem_env
    $ source miem_env/bin/activate
    $ cd docs
    $ pip install -r requirements.txt
    $ cd .. && mkdir build && cd build
    $ cmake -D MIEM_BUILD_DOCS=ON ..
    $ make docs
    $ open docs/sphinx/index.html
