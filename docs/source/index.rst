.. MIEM documentation HTML titles
..
.. # (over and under) for module headings
.. = for sections
.. - for subsections
.. ^ for subsubsections
.. ~ for subsubsubsections
.. " for paragraphs

================================
Welcome to MIEM's documentation!
================================

**MIEM** (Model Independent Emissions Module) is a header-only C++ library for
computing atmospheric emission fluxes, designed to plug into host models such
as MPAS-A and CAM as part of the `MUSICA <https://github.com/NCAR/musica>`_
framework developed at `NSF NCAR <https://ncar.ucar.edu/>`_.

.. grid:: 1 1 2 2
    :gutter: 2

    .. grid-item-card:: Getting started
        :img-top: _static/index_getting_started.svg
        :link: getting_started
        :link-type: doc

        Check out the getting started guide to build and install MIEM.

    .. grid-item-card::  User guide
        :img-top: _static/index_user_guide.svg
        :link: user_guide/index
        :link-type: doc

        Learn how to drive MIEM from a host model here!

    .. grid-item-card::  API reference
        :img-top: _static/index_api.svg
        :link: api/index
        :link-type: doc

        The source code for MIEM is heavily documented. This reference will help you extend MIEM for your needs.

    .. grid-item-card::  Contributors guide
        :img-top: _static/index_contribute.svg
        :link: contributing/index
        :link-type: doc

        If you'd like to contribute some new science code or update the docs,
        checkout the contributors guide!


.. toctree::
   :maxdepth: 2
   :caption: Contents:

   getting_started
   user_guide/index
   api/index
   contributing/index
   citing_and_bibliography/index

Indices and tables
==================

* :ref:`genindex`
* :ref:`search`
