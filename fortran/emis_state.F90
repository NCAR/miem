module miem_emis_state_mod
  use iso_c_binding
  use miem_util_mod
  implicit none
  private

  type, public :: emis_state_t
    type(c_ptr), private :: ptr = c_null_ptr
    integer :: n_species = 0
    integer :: n_cells = 0
    integer :: n_vert_levels = 0
    real(c_double), pointer :: surface_flux(:,:) => null()
    real(c_double), pointer :: tendency(:,:,:) => null()
    integer(c_int), pointer :: emis_to_chem_idx(:) => null()
  contains
    procedure :: update_references => emis_state_update_references
    procedure :: delete => emis_state_delete
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
    type(error_t), intent(inout) :: error

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

end module miem_emis_state_mod
