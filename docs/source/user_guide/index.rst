##########
User Guide
##########

MIEM is driven directly by a host model (e.g. MPAS-A or CAM) in C++. The
examples below show how to assemble emission sources and run an emissions
timestep. Species mapping and inventory translation are handled upstream by
`MechanismConfiguration <https://github.com/NCAR/MechanismConfiguration>`_ and
``musica::Translate()``, so by the time data reaches MIEM the species are
already resolved.

If you find our examples are lacking for your needs, please
`file an issue <https://github.com/NCAR/miem/issues/new>`_ and request the
kind of example you'd like.

.. toctree::
   :maxdepth: 1
   :caption: Contents:

   quickstart
