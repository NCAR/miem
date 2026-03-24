module miem_mod
  use iso_c_binding
  use miem_util_mod
  use miem_emis_state_mod
  implicit none
  private

  type, public :: miem_t
    type(c_ptr), private :: ptr = c_null_ptr
    integer, private :: n_cells_ = 0
    integer, private :: n_vert_levels_ = 0
  contains
    procedure :: init => miem_init
    procedure :: run => miem_run
    procedure :: resolve_host_indices => miem_resolve_host_indices
    procedure :: num_species => miem_num_species
    procedure :: get_n_cells => miem_get_n_cells
    procedure :: get_n_vert_levels => miem_get_n_vert_levels
    procedure :: finalize => miem_finalize
  end type miem_t

  public :: miem_query_species_count

  ! C function interfaces
  interface
    function CreateMIEM_c(config_path, n_cells, n_vert_levels, error) &
        bind(C, name="CreateMIEM")
      import :: c_ptr, c_char, c_int
      character(kind=c_char), intent(in) :: config_path(*)
      integer(c_int), value :: n_cells
      integer(c_int), value :: n_vert_levels
      type(c_ptr), value :: error
      type(c_ptr) :: CreateMIEM_c
    end function

    subroutine DeleteMIEM_c(miem, error) bind(C, name="DeleteMIEM")
      import :: c_ptr
      type(c_ptr), value :: miem
      type(c_ptr), value :: error
    end subroutine

    function MIEMRun_c(miem, time, air_density, layer_thickness, &
        n_atm_elements, error) bind(C, name="MIEMRun")
      import :: c_ptr, c_double, c_int
      type(c_ptr), value :: miem
      real(c_double), value :: time
      type(c_ptr), value :: air_density
      type(c_ptr), value :: layer_thickness
      integer(c_int), value :: n_atm_elements
      type(c_ptr), value :: error
      type(c_ptr) :: MIEMRun_c
    end function

    subroutine MIEMResolveHostIndices_c(miem, host_names, n_host, indices, error) &
        bind(C, name="MIEMResolveHostIndices")
      import :: c_ptr, c_int
      type(c_ptr), value :: miem
      type(c_ptr), value :: host_names
      integer(c_int), value :: n_host
      type(c_ptr), value :: indices
      type(c_ptr), value :: error
    end subroutine

    function MIEMGetNumSpecies_c(miem) bind(C, name="MIEMGetNumSpecies")
      import :: c_ptr, c_int
      type(c_ptr), value :: miem
      integer(c_int) :: MIEMGetNumSpecies_c
    end function

    function MIEMQuerySpeciesCount_c(config_path, error) &
        bind(C, name="MIEMQuerySpeciesCount")
      import :: c_ptr, c_char, c_int
      character(kind=c_char), intent(in) :: config_path(*)
      type(c_ptr), value :: error
      integer(c_int) :: MIEMQuerySpeciesCount_c
    end function

    subroutine MIEMQuerySpeciesNames_c(config_path, names, max_names, error) &
        bind(C, name="MIEMQuerySpeciesNames")
      import :: c_ptr, c_char, c_int
      character(kind=c_char), intent(in) :: config_path(*)
      type(c_ptr), value :: names
      integer(c_int), value :: max_names
      type(c_ptr), value :: error
    end subroutine
  end interface

contains

  subroutine miem_init(this, config_path, n_cells, n_vert_levels, error)
    class(miem_t), intent(inout) :: this
    character(len=*), intent(in) :: config_path
    integer, intent(in) :: n_cells
    integer, intent(in) :: n_vert_levels
    type(error_t), intent(inout), target :: error

    character(len=len_trim(config_path)+1, kind=c_char) :: c_config

    c_config = f_to_c_string(config_path)
    this%ptr = CreateMIEM_c(c_config, int(n_cells, c_int), &
                            int(n_vert_levels, c_int), c_loc(error))
    this%n_cells_ = n_cells
    this%n_vert_levels_ = n_vert_levels
  end subroutine

  subroutine miem_run(this, time_current, air_density, layer_thickness, &
                      state, error)
    class(miem_t), intent(inout) :: this
    real(c_double), intent(in) :: time_current
    real(c_double), intent(in), target :: air_density(:)
    real(c_double), intent(in), target :: layer_thickness(:)
    type(emis_state_t), intent(out) :: state
    type(error_t), intent(inout), target :: error

    integer(c_int) :: n_atm

    type(c_ptr) :: state_ptr

    n_atm = int(size(air_density), c_int)
    state_ptr = MIEMRun_c(this%ptr, time_current, &
                           c_loc(air_density(1)), c_loc(layer_thickness(1)), &
                           n_atm, c_loc(error))
    call state%set_ptr(state_ptr)

    if (error_is_success(error) .and. state%is_associated()) then
      call state%update_references()
    end if
  end subroutine

  subroutine miem_resolve_host_indices(this, host_species, indices, error)
    class(miem_t), intent(inout) :: this
    character(len=*), intent(in) :: host_species(:)
    integer(c_int), intent(out), target :: indices(:)
    type(error_t), intent(inout), target :: error

    type(c_ptr), allocatable, target :: c_names(:)
    character(len=MIEM_MAX_NAME_LEN, kind=c_char), allocatable, target :: c_strings(:)
    integer :: i, n

    n = size(host_species)
    allocate(c_names(n))
    allocate(c_strings(n))

    do i = 1, n
      c_strings(i) = f_to_c_string(host_species(i))
      c_names(i) = c_loc(c_strings(i))
    end do

    call MIEMResolveHostIndices_c(this%ptr, c_loc(c_names(1)), &
                                  int(n, c_int), c_loc(indices(1)), c_loc(error))
  end subroutine

  function miem_num_species(this) result(n)
    class(miem_t), intent(in) :: this
    integer :: n
    n = MIEMGetNumSpecies_c(this%ptr)
  end function

  function miem_query_species_count(config_path, error) result(n)
    character(len=*), intent(in) :: config_path
    type(error_t), intent(inout), target :: error
    integer :: n

    character(len=len_trim(config_path)+1, kind=c_char) :: c_config

    c_config = f_to_c_string(config_path)
    n = MIEMQuerySpeciesCount_c(c_config, c_loc(error))
  end function

  function miem_get_n_cells(this) result(n)
    class(miem_t), intent(in) :: this
    integer :: n
    n = this%n_cells_
  end function

  function miem_get_n_vert_levels(this) result(n)
    class(miem_t), intent(in) :: this
    integer :: n
    n = this%n_vert_levels_
  end function

  subroutine miem_finalize(this, error)
    class(miem_t), intent(inout) :: this
    type(error_t), intent(inout), target :: error

    if (c_associated(this%ptr)) then
      call DeleteMIEM_c(this%ptr, c_loc(error))
      this%ptr = c_null_ptr
    end if
  end subroutine

end module miem_mod
