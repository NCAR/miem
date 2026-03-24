module miem_emis_state_mod
  use iso_c_binding
  use miem_util_mod
  implicit none
  private

  type, public :: emis_state_t
    type(c_ptr), private :: ptr = c_null_ptr
    integer, private :: n_species = 0
    integer, private :: n_cells = 0
    integer, private :: n_vert_levels = 0
    real(c_double), pointer :: surface_flux(:,:) => null()
    real(c_double), pointer :: tendency(:,:,:) => null()
    integer(c_int), pointer :: emis_to_chem_idx(:) => null()
  contains
    procedure :: update_references => emis_state_update_references
    procedure :: delete => emis_state_delete
    procedure :: set_ptr => emis_state_set_ptr
    procedure :: is_associated => emis_state_is_associated
    procedure :: get_n_species => emis_state_get_n_species
    procedure :: get_n_cells => emis_state_get_n_cells
    procedure :: get_n_vert_levels => emis_state_get_n_vert_levels
    procedure :: get_sector_count => emis_state_get_sector_count
    procedure :: get_sector_flux => emis_state_get_sector_flux
    final :: emis_state_finalize
  end type emis_state_t

  ! C function interfaces
  interface
    function MIEMGetSurfaceFlux_c(state) bind(C, name="MIEMGetSurfaceFlux")
      import :: c_ptr
      type(c_ptr), value :: state
      type(c_ptr) :: MIEMGetSurfaceFlux_c
    end function

    function MIEMGetTendency_c(state) bind(C, name="MIEMGetTendency")
      import :: c_ptr
      type(c_ptr), value :: state
      type(c_ptr) :: MIEMGetTendency_c
    end function

    function MIEMGetEmisToChemIdx_c(state) bind(C, name="MIEMGetEmisToChemIdx")
      import :: c_ptr
      type(c_ptr), value :: state
      type(c_ptr) :: MIEMGetEmisToChemIdx_c
    end function

    function MIEMGetStateNumSpecies_c(state) bind(C, name="MIEMGetStateNumSpecies")
      import :: c_ptr, c_int
      type(c_ptr), value :: state
      integer(c_int) :: MIEMGetStateNumSpecies_c
    end function

    function MIEMGetStateNumCells_c(state) bind(C, name="MIEMGetStateNumCells")
      import :: c_ptr, c_int
      type(c_ptr), value :: state
      integer(c_int) :: MIEMGetStateNumCells_c
    end function

    function MIEMGetStateNumVertLevels_c(state) bind(C, name="MIEMGetStateNumVertLevels")
      import :: c_ptr, c_int
      type(c_ptr), value :: state
      integer(c_int) :: MIEMGetStateNumVertLevels_c
    end function

    subroutine DeleteMIEMState_c(state, error) bind(C, name="DeleteMIEMState")
      import :: c_ptr
      type(c_ptr), value :: state
      type(c_ptr), value :: error
    end subroutine

    function MIEMGetSectorCount_c(state) bind(C, name="MIEMGetSectorCount")
      import :: c_ptr, c_int
      type(c_ptr), value :: state
      integer(c_int) :: MIEMGetSectorCount_c
    end function

    function MIEMGetSectorFlux_c(state, sector_name, error) &
        bind(C, name="MIEMGetSectorFlux")
      import :: c_ptr, c_char
      type(c_ptr), value :: state
      character(kind=c_char), intent(in) :: sector_name(*)
      type(c_ptr), value :: error
      type(c_ptr) :: MIEMGetSectorFlux_c
    end function
  end interface

contains

  subroutine emis_state_update_references(this)
    class(emis_state_t), intent(inout) :: this
    type(c_ptr) :: data_ptr

    if (.not. c_associated(this%ptr)) return

    this%n_species = MIEMGetStateNumSpecies_c(this%ptr)
    this%n_cells = MIEMGetStateNumCells_c(this%ptr)
    this%n_vert_levels = MIEMGetStateNumVertLevels_c(this%ptr)

    ! Surface flux: (n_species, n_cells) — species-major in C, becomes (n_cells, n_species)
    data_ptr = MIEMGetSurfaceFlux_c(this%ptr)
    if (c_associated(data_ptr) .and. this%n_species > 0 .and. this%n_cells > 0) then
      call c_f_pointer(data_ptr, this%surface_flux, [this%n_cells, this%n_species])
    end if

    ! Tendency: (n_species * n_vert_levels, n_cells)
    data_ptr = MIEMGetTendency_c(this%ptr)
    if (c_associated(data_ptr) .and. this%n_species > 0 .and. this%n_cells > 0 &
        .and. this%n_vert_levels > 0) then
      call c_f_pointer(data_ptr, this%tendency, &
          [this%n_cells, this%n_vert_levels, this%n_species])
    end if

    ! Emissions-to-chemistry index mapping
    data_ptr = MIEMGetEmisToChemIdx_c(this%ptr)
    if (c_associated(data_ptr) .and. this%n_species > 0) then
      call c_f_pointer(data_ptr, this%emis_to_chem_idx, [this%n_species])
    end if
  end subroutine

  subroutine emis_state_delete(this, error)
    class(emis_state_t), intent(inout) :: this
    type(error_t), intent(inout), target :: error

    if (c_associated(this%ptr)) then
      call DeleteMIEMState_c(this%ptr, c_loc(error))
      this%ptr = c_null_ptr
    end if

    this%surface_flux => null()
    this%tendency => null()
    this%emis_to_chem_idx => null()
    this%n_species = 0
    this%n_cells = 0
    this%n_vert_levels = 0
  end subroutine

  subroutine emis_state_set_ptr(this, new_ptr)
    class(emis_state_t), intent(inout) :: this
    type(c_ptr), intent(in) :: new_ptr
    this%ptr = new_ptr
  end subroutine

  function emis_state_is_associated(this) result(assoc)
    class(emis_state_t), intent(in) :: this
    logical :: assoc
    assoc = c_associated(this%ptr)
  end function

  function emis_state_get_n_species(this) result(n)
    class(emis_state_t), intent(in) :: this
    integer :: n
    n = this%n_species
  end function

  function emis_state_get_n_cells(this) result(n)
    class(emis_state_t), intent(in) :: this
    integer :: n
    n = this%n_cells
  end function

  function emis_state_get_n_vert_levels(this) result(n)
    class(emis_state_t), intent(in) :: this
    integer :: n
    n = this%n_vert_levels
  end function

  function emis_state_get_sector_count(this) result(n)
    class(emis_state_t), intent(in) :: this
    integer :: n
    if (c_associated(this%ptr)) then
      n = MIEMGetSectorCount_c(this%ptr)
    else
      n = 0
    end if
  end function

  subroutine emis_state_get_sector_flux(this, sector_name, flux_ptr, error)
    class(emis_state_t), intent(in) :: this
    character(len=*), intent(in) :: sector_name
    real(c_double), pointer, intent(out) :: flux_ptr(:,:)
    type(error_t), intent(inout), target :: error
    type(c_ptr) :: data_ptr
    character(len=len_trim(sector_name)+1, kind=c_char) :: c_name

    nullify(flux_ptr)
    if (.not. c_associated(this%ptr)) return

    c_name = f_to_c_string(sector_name)
    data_ptr = MIEMGetSectorFlux_c(this%ptr, c_name, c_loc(error))
    if (c_associated(data_ptr) .and. this%n_species > 0 .and. this%n_cells > 0) then
      call c_f_pointer(data_ptr, flux_ptr, [this%n_cells, this%n_species])
    end if
  end subroutine

  ! FINAL procedure — automatically frees C++ heap allocation when
  ! the Fortran object goes out of scope
  subroutine emis_state_finalize(this)
    type(emis_state_t), intent(inout) :: this

    if (c_associated(this%ptr)) then
      call DeleteMIEMState_c(this%ptr, c_null_ptr)
      this%ptr = c_null_ptr
    end if
    this%surface_flux => null()
    this%tendency => null()
    this%emis_to_chem_idx => null()
  end subroutine

end module miem_emis_state_mod
