module miem_util_mod
  use iso_c_binding
  implicit none

  integer, parameter, public :: MIEM_MAX_NAME_LEN = 64

  ! Error type matching C MIEM_Error struct
  type, bind(C) :: error_t
    integer(c_int) :: code = 0
    character(kind=c_char) :: category(64) = c_null_char
    character(kind=c_char) :: message(256) = c_null_char
  end type error_t

contains

  function error_is_success(error) result(success)
    type(error_t), intent(in) :: error
    logical :: success
    success = (error%code == 0)
  end function

  function error_message(error) result(msg)
    type(error_t), intent(in) :: error
    character(len=255) :: msg
    integer :: i

    msg = ' '
    do i = 1, 255
      if (error%message(i) == c_null_char) exit
      msg(i:i) = error%message(i)
    end do
  end function

  function error_category(error) result(cat)
    type(error_t), intent(in) :: error
    character(len=63) :: cat
    integer :: i

    cat = ' '
    do i = 1, 63
      if (error%category(i) == c_null_char) exit
      cat(i:i) = error%category(i)
    end do
  end function

  ! Convert a Fortran string to a null-terminated C string
  function f_to_c_string(f_str) result(c_str)
    character(len=*), intent(in) :: f_str
    character(len=len_trim(f_str)+1, kind=c_char) :: c_str

    c_str = trim(f_str) // c_null_char
  end function

end module miem_util_mod
