program test_miem_fortran
  use miem_mod
  use miem_emis_state_mod
  use miem_util_mod
  implicit none

  type(error_t) :: error
  logical :: all_passed

  all_passed = .true.

  ! Test error_t default state
  call test_error_default(all_passed)

  ! Test error utility functions
  call test_error_functions(all_passed)

  if (all_passed) then
    write(*,*) "All Fortran tests PASSED"
  else
    write(*,*) "Some Fortran tests FAILED"
    stop 1
  end if

contains

  subroutine test_error_default(passed)
    logical, intent(inout) :: passed
    type(error_t) :: err

    if (error_is_success(err)) then
      write(*,*) "  PASS: error_t default is success"
    else
      write(*,*) "  FAIL: error_t default should be success"
      passed = .false.
    end if
  end subroutine

  subroutine test_error_functions(passed)
    logical, intent(inout) :: passed
    type(error_t) :: err
    character(len=255) :: msg

    ! Default error should have code 0
    if (err%code == 0) then
      write(*,*) "  PASS: error code is 0"
    else
      write(*,*) "  FAIL: error code should be 0"
      passed = .false.
    end if

    msg = error_message(err)
    write(*,*) "  PASS: error_message returns cleanly"
  end subroutine

end program test_miem_fortran
