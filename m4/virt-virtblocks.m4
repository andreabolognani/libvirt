AC_DEFUN([LIBVIRT_ARG_VIRTBLOCKS], [
  LIBVIRT_ARG_WITH([VIRTBLOCKS], [use Virt Blocks (rust|golang)], [no])
])

AC_DEFUN([LIBVIRT_CHECK_VIRTBLOCKS], [
  case "$with_virtblocks" in
    rust)
      AC_DEFINE_UNQUOTED([WITH_VIRTBLOCKS_RUST], 1, [Virt Blocks with Rust])
      AC_DEFINE_UNQUOTED([WITH_VIRTBLOCKS], 1, [Virt Blocks])
      ;;
    golang)
      AC_DEFINE_UNQUOTED([WITH_VIRTBLOCKS_GOLANG], 1, [Virt Blocks with Go])
      AC_DEFINE_UNQUOTED([WITH_VIRTBLOCKS], 1, [Virt Blocks])
      ;;
    no) AC_MSG_ERROR([missing Virt Blocks selection]) ;;
    *) AC_MSG_ERROR([invalid Virt Blocks selection]) ;;
  esac
  AM_CONDITIONAL([WITH_VIRTBLOCKS_RUST], [test "$with_virtblocks" = "rust"])
  AM_CONDITIONAL([WITH_VIRTBLOCKS_GOLANG], [test "$with_virtblocks" = "golang"])
  AM_CONDITIONAL([WITH_VIRTBLOCKS], [test "$with_virtblocks" != "no"])
])

AC_DEFUN([LIBVIRT_RESULT_VIRTBLOCKS], [
  AC_MSG_NOTICE([       Virt Blocks: $with_virtblocks])
])
