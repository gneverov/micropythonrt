# The object files from libc.a that definitely must be linked into firmware.elf.
# The linker may at more object files, but this set below is guaranteed to be available for dynamically linking libraries.
set(LIBC_OBJECTS
    aeabi_memset.c.o
    bzero.c.o
    # libc_argz_argz_add.c.o
    # libc_argz_argz_add_sep.c.o
    # libc_argz_argz_append.c.o
    # libc_argz_argz_count.c.o
    # libc_argz_argz_create.c.o
    # libc_argz_argz_create_sep.c.o
    # libc_argz_argz_delete.c.o
    # libc_argz_argz_extract.c.o
    # libc_argz_argz_insert.c.o
    # libc_argz_argz_next.c.o
    # libc_argz_argz_replace.c.o
    # libc_argz_argz_stringify.c.o
    # libc_argz_buf_findstr.c.o
    # libc_argz_envz_add.c.o
    # libc_argz_envz_entry.c.o
    # libc_argz_envz_get.c.o
    # libc_argz_envz_merge.c.o
    # libc_argz_envz_remove.c.o
    # libc_argz_envz_strip.c.o
    libc_ctype_caseconv.c.o
    libc_ctype_ctype_.c.o
    libc_ctype_ctype_table.c.o
    libc_ctype_ctype_wide.c.o
    libc_ctype_isalnum.c.o
    libc_ctype_isalnum_l.c.o
    libc_ctype_isalpha.c.o
    libc_ctype_isalpha_l.c.o
    libc_ctype_isascii.c.o
    libc_ctype_isascii_l.c.o
    libc_ctype_isblank.c.o
    libc_ctype_isblank_l.c.o
    libc_ctype_iscntrl.c.o
    libc_ctype_iscntrl_l.c.o
    libc_ctype_isdigit.c.o
    libc_ctype_isdigit_l.c.o
    libc_ctype_isgraph.c.o
    libc_ctype_isgraph_l.c.o
    libc_ctype_islower.c.o
    libc_ctype_islower_l.c.o
    libc_ctype_isprint.c.o
    libc_ctype_isprint_l.c.o
    libc_ctype_ispunct.c.o
    libc_ctype_ispunct_l.c.o
    libc_ctype_isspace.c.o
    libc_ctype_isspace_l.c.o
    libc_ctype_isupper.c.o
    libc_ctype_isupper_l.c.o
    libc_ctype_iswalnum.c.o
    libc_ctype_iswalnum_l.c.o
    libc_ctype_iswalpha.c.o
    libc_ctype_iswalpha_l.c.o
    libc_ctype_iswblank.c.o
    libc_ctype_iswblank_l.c.o
    libc_ctype_iswcntrl.c.o
    libc_ctype_iswcntrl_l.c.o
    libc_ctype_iswctype.c.o
    libc_ctype_iswctype_l.c.o
    libc_ctype_iswdigit.c.o
    libc_ctype_iswdigit_l.c.o
    libc_ctype_iswgraph.c.o
    libc_ctype_iswgraph_l.c.o
    libc_ctype_iswlower.c.o
    libc_ctype_iswlower_l.c.o
    libc_ctype_iswprint.c.o
    libc_ctype_iswprint_l.c.o
    libc_ctype_iswpunct.c.o
    libc_ctype_iswpunct_l.c.o
    libc_ctype_iswspace.c.o
    libc_ctype_iswspace_l.c.o
    libc_ctype_iswupper.c.o
    libc_ctype_iswupper_l.c.o
    libc_ctype_iswxdigit.c.o
    libc_ctype_iswxdigit_l.c.o
    libc_ctype_isxdigit.c.o
    libc_ctype_isxdigit_l.c.o
    libc_ctype_jp2uc.c.o
    libc_ctype_toascii.c.o
    libc_ctype_toascii_l.c.o
    libc_ctype_tolower.c.o
    libc_ctype_tolower_l.c.o
    libc_ctype_toupper.c.o
    libc_ctype_toupper_l.c.o
    libc_ctype_towctrans.c.o
    libc_ctype_towctrans_l.c.o
    libc_ctype_towlower.c.o
    libc_ctype_towlower_l.c.o
    libc_ctype_towupper.c.o
    libc_ctype_towupper_l.c.o
    libc_ctype_wctrans.c.o
    libc_ctype_wctrans_l.c.o
    libc_ctype_wctype.c.o
    libc_ctype_wctype_l.c.o
    libc_errno_errno.c.o
    # libc_iconv_iconv_close.c.o
    # libc_iconv_iconv.c.o
    # libc_iconv_iconv_open.c.o
    # libc_locale_duplocale.c.o
    # libc_locale_freelocale.c.o
    # libc_locale_getlocalename_l.c.o
    # libc_locale_localeconv.c.o
    # libc_locale_locale_ctype_ptr.c.o
    # libc_locale_locale_ctype_ptr_l.c.o
    # libc_locale_localedata.c.o
    # libc_locale_locale_mb_cur_max.c.o
    # libc_locale_locale_names.c.o
    # libc_locale_newlocale.c.o
    # libc_locale_nl_langinfo.c.o
    # libc_locale_setlocale.c.o
    # libc_locale_timedata.c.o
    # libc_locale_uselocale.c.o
    # libc_misc_ffs.c.o
    # libc_misc_fini.c.o
    # libc_misc_init.c.o
    # libc_misc_lock.c.o
    # libc_misc_unctrl.c.o
    # libc_picolib_dso_handle.c.o
    # libc_picolib_getauxval.c.o
    # libc_picolib_inittls.c.o
    # libc_picolib_machine_arm_interrupt.c.o
    # libc_picolib_machine_arm_read_tp.S.o
    # libc_picolib_machine_arm_set_tls.c.o
    # libc_picolib_machine_arm_tls.c.o
    # libc_picolib_picosbrk.c.o
    libc_posix_basename.c.o
    # libc_posix_collcmp.c.o
    libc_posix_dirname.c.o
    # libc_posix_fnmatch.c.o
    # libc_posix_fpathconf.c.o
    # libc_posix_pathconf.c.o
    # libc_posix_regcomp.c.o
    # libc_posix_regerror.c.o
    # libc_posix_regexec.c.o
    # libc_posix_regfree.c.o
    # libc_search_bsd_qsort_r.c.o
    libc_search_bsearch.c.o
    # libc_search_hash_bigkey.c.o
    # libc_search_hash_buf.c.o
    # libc_search_hash.c.o
    # libc_search_hash_func.c.o
    # libc_search_hash_log2.c.o
    # libc_search_hash_page.c.o
    # libc_search_hcreate.c.o
    # libc_search_hcreate_r.c.o
    # libc_search_ndbm.c.o
    libc_search_qsort.c.o
    # libc_search_qsort_r.c.o
    # libc_search_tdelete.c.o
    # libc_search_tdestroy.c.o
    # libc_search_tfind.c.o
    # libc_search_tsearch.c.o
    # libc_search_twalk.c.o
    libc_signal_psignal.c.o
    libc_signal_sig2str.c.o
    libc_signal_signal.c.o
    # libc_ssp_chk_fail.c.o
    # libc_ssp_gets_chk.c.o
    # libc_ssp_memcpy_chk.c.o
    # libc_ssp_memmove_chk.c.o
    # libc_ssp_mempcpy_chk.c.o
    # libc_ssp_memset_chk.c.o
    # libc_ssp_snprintf_chk.c.o
    # libc_ssp_sprintf_chk.c.o
    # libc_ssp_stack_protector.c.o
    # libc_ssp_stpcpy_chk.c.o
    # libc_ssp_stpncpy_chk.c.o
    # libc_ssp_strcat_chk.c.o
    # libc_ssp_strcpy_chk.c.o
    # libc_ssp_strncat_chk.c.o
    # libc_ssp_strncpy_chk.c.o
    # libc_ssp_vsnprintf_chk.c.o
    # libc_ssp_vsprintf_chk.c.o
    libc_stdlib_a64l.c.o
    libc_stdlib_abort.c.o
    libc_stdlib_abs.c.o
    libc_stdlib_aligned_alloc.c.o
    # libc_stdlib_arc4random.c.o
    # libc_stdlib_arc4random_uniform.c.o
    libc_stdlib_assert.c.o
    libc_stdlib_assert_func.c.o
    libc_stdlib_assert_no_arg.c.o
    libc_stdlib_atof.c.o
    libc_stdlib_atoff.c.o
    libc_stdlib_atoi.c.o
    libc_stdlib_atol.c.o
    libc_stdlib_atoll.c.o
    libc_stdlib_btowc.c.o
    libc_stdlib_div.c.o
    # libc_stdlib_drand48.c.o
    libc_stdlib_environ.c.o
    # libc_stdlib_eprintf.c.o
    # libc_stdlib_erand48.c.o
    libc_stdlib__Exit.c.o
    libc_stdlib_getenv.c.o
    # libc_stdlib_getenv_r.c.o
    # libc_stdlib_getopt.c.o
    # libc_stdlib_getsubopt.c.o
    # libc_stdlib_ignore_handler_s.c.o
    libc_stdlib_imaxabs.c.o
    libc_stdlib_imaxdiv.c.o
    libc_stdlib_itoa.c.o
    # libc_stdlib_jrand48.c.o
    libc_stdlib_l64a.c.o
    libc_stdlib_labs.c.o
    # libc_stdlib_lcong48.c.o
    libc_stdlib_ldiv.c.o
    libc_stdlib_llabs.c.o
    libc_stdlib_lldiv.c.o
    # libc_stdlib_lrand48.c.o
    libc_stdlib_mblen.c.o
    libc_stdlib_mbrlen.c.o
    libc_stdlib_mbrtowc.c.o
    libc_stdlib_mbsinit.c.o
    libc_stdlib_mbsnrtowcs.c.o
    libc_stdlib_mbsrtowcs.c.o
    libc_stdlib_mbstowcs.c.o
    libc_stdlib_mbtowc.c.o
    libc_stdlib_mbtowc_r.c.o
    # libc_stdlib_mrand48.c.o
    # libc_stdlib_nrand48.c.o
    # libc_stdlib_pico-atexit.c.o
    # libc_stdlib_pico-cxa-atexit.c.o
    # libc_stdlib_pico-exit.c.o
    # libc_stdlib_pico-exitprocs.c.o
    # libc_stdlib_pico-onexit.c.o
    libc_stdlib_putenv.c.o
    # libc_stdlib_rand48.c.o
    # libc_stdlib_rand.c.o
    # libc_stdlib_random.c.o
    # libc_stdlib_rand_r.c.o
    # libc_stdlib_reallocarray.c.o
    # libc_stdlib_reallocf.c.o
    # libc_stdlib_rpmatch.c.o
    # libc_stdlib_sb_charsets.c.o
    # libc_stdlib_seed48.c.o
    # libc_stdlib_set_constraint_handler_s.c.o
    libc_stdlib_setenv.c.o
    # libc_stdlib_srand48.c.o
    # libc_stdlib_srand.c.o
    # libc_stdlib_srandom.c.o
    # libc_stdlib_system.c.o
    libc_stdlib_utoa.c.o
    libc_stdlib_wcrtomb.c.o
    libc_stdlib_wcsnrtombs.c.o
    libc_stdlib_wcsrtombs.c.o
    libc_stdlib_wcstombs.c.o
    libc_stdlib_wctob.c.o
    libc_stdlib_wctomb.c.o
    libc_stdlib_wctomb_r.c.o
    libc_string_bcmp.c.o
    libc_string_bcopy.c.o
    libc_string_explicit_bzero.c.o
    libc_string_ffsl.c.o
    libc_string_ffsll.c.o
    libc_string_fls.c.o
    libc_string_flsl.c.o
    libc_string_flsll.c.o
    libc_string_gnu_basename.c.o
    libc_string_index.c.o
    libc_string_memccpy.c.o
    libc_string_memcmp.c.o
    libc_string_memcpy_s.c.o
    libc_string_memmem.c.o
    libc_string_memmove_s.c.o
    libc_string_mempcpy.c.o
    libc_string_memrchr.c.o
    libc_string_memset_explicit.c.o
    libc_string_memset_s.c.o
    libc_string_rawmemchr.c.o
    libc_string_rindex.c.o
    libc_string_stpcpy.c.o
    libc_string_stpncpy.c.o
    libc_string_strcasecmp.c.o
    libc_string_strcasecmp_l.c.o
    libc_string_strcasestr.c.o
    libc_string_strcat.c.o
    libc_string_strcat_s.c.o
    libc_string_strchr.c.o
    libc_string_strchrnul.c.o
    libc_string_strcoll.c.o
    libc_string_strcoll_l.c.o
    libc_string_strcpy_s.c.o
    libc_string_strcspn.c.o
    libc_string_strdup.c.o
    libc_string_strerror.c.o
    libc_string_strerrorlen_s.c.o
    libc_string_strerror_r.c.o
    libc_string_strerror_s.c.o
    libc_string_strlcat.c.o
    libc_string_strlcpy.c.o
    libc_string_strlwr.c.o
    libc_string_strncasecmp.c.o
    libc_string_strncasecmp_l.c.o
    libc_string_strncat.c.o
    libc_string_strncat_s.c.o
    libc_string_strncmp.c.o
    libc_string_strncpy.c.o
    libc_string_strncpy_s.c.o
    libc_string_strndup.c.o
    libc_string_strnlen.c.o
    libc_string_strnlen_s.c.o
    libc_string_strnstr.c.o
    libc_string_strpbrk.c.o
    libc_string_strrchr.c.o
    libc_string_strsep.c.o
    libc_string_strsignal.c.o
    libc_string_strspn.c.o
    libc_string_strstr.c.o
    libc_string_strtok.c.o
    libc_string_strtok_r.c.o
    libc_string_strupr.c.o
    libc_string_strverscmp.c.o
    libc_string_strxfrm.c.o
    libc_string_strxfrm_l.c.o
    libc_string_swab.c.o
    libc_string_timingsafe_bcmp.c.o
    libc_string_timingsafe_memcmp.c.o
    libc_string_wcpcpy.c.o
    libc_string_wcpncpy.c.o
    libc_string_wcscasecmp.c.o
    libc_string_wcscasecmp_l.c.o
    libc_string_wcscat.c.o
    libc_string_wcschr.c.o
    libc_string_wcscmp.c.o
    libc_string_wcscoll.c.o
    libc_string_wcscoll_l.c.o
    libc_string_wcscpy.c.o
    libc_string_wcscspn.c.o
    libc_string_wcsdup.c.o
    libc_string_wcslcat.c.o
    libc_string_wcslcpy.c.o
    libc_string_wcslen.c.o
    libc_string_wcsncasecmp.c.o
    libc_string_wcsncasecmp_l.c.o
    libc_string_wcsncat.c.o
    libc_string_wcsncmp.c.o
    libc_string_wcsncpy.c.o
    libc_string_wcsnlen.c.o
    libc_string_wcspbrk.c.o
    libc_string_wcsrchr.c.o
    libc_string_wcsspn.c.o
    libc_string_wcsstr.c.o
    libc_string_wcstok.c.o
    libc_string_wcswidth.c.o
    libc_string_wcsxfrm.c.o
    libc_string_wcsxfrm_l.c.o
    libc_string_wcwidth.c.o
    libc_string_wmemchr.c.o
    libc_string_wmemcmp.c.o
    libc_string_wmemcpy.c.o
    libc_string_wmemmove.c.o
    libc_string_wmempcpy.c.o
    libc_string_wmemset.c.o
    # libc_string_xpg_strerror_r.c.o
    libc_time_asctime.c.o
    libc_time_asctime_r.c.o
    libc_time_clock.c.o
    libc_time_ctime.c.o
    libc_time_ctime_r.c.o
    libc_time_difftime.c.o
    libc_time_gettzinfo.c.o
    libc_time_gmtime.c.o
    libc_time_gmtime_r.c.o
    libc_time_lcltime_buf.c.o
    libc_time_lcltime.c.o
    libc_time_lcltime_r.c.o
    libc_time_mktime.c.o
    libc_time_month_lengths.c.o
    libc_time_strftime.c.o
    libc_time_strptime.c.o
    libc_time_time.c.o
    libc_time_tzcalc_limits.c.o
    libc_time_tzset.c.o
    libc_time_tzvars.c.o
    libc_time_wcsftime.c.o
    libc_tinystdio_asnprintf.c.o
    libc_tinystdio_asprintf.c.o
    libc_tinystdio_atod_ryu.c.o
    libc_tinystdio_atof_ryu.c.o
    libc_tinystdio_atold_engine.c.o
    libc_tinystdio_atomic_load.c.o
    libc_tinystdio_bufio.c.o
    libc_tinystdio_clearerr.c.o
    libc_tinystdio_compare_exchange.c.o
    libc_tinystdio_dtoa_ryu.c.o
    libc_tinystdio_dtox_engine.c.o
    libc_tinystdio_ecvt.c.o
    libc_tinystdio_ecvtf.c.o
    libc_tinystdio_ecvtf_r.c.o
    libc_tinystdio_ecvtl.c.o
    libc_tinystdio_ecvtl_r.c.o
    libc_tinystdio_ecvt_r.c.o
    libc_tinystdio_exchange.c.o
    libc_tinystdio_fclose.c.o
    libc_tinystdio_fcvt.c.o
    libc_tinystdio_fcvtf.c.o
    libc_tinystdio_fcvtf_r.c.o
    libc_tinystdio_fcvtl.c.o
    libc_tinystdio_fcvtl_r.c.o
    libc_tinystdio_fcvt_r.c.o
    libc_tinystdio_fdevopen.c.o
    libc_tinystdio_fdopen.c.o
    libc_tinystdio_feof.c.o
    libc_tinystdio_ferror.c.o
    libc_tinystdio_fflush.c.o
    libc_tinystdio_fgetc.c.o
    libc_tinystdio_fgetpos.c.o
    libc_tinystdio_fgets.c.o
    libc_tinystdio_fgetwc.c.o
    libc_tinystdio_fgetws.c.o
    libc_tinystdio_fileno.c.o
    libc_tinystdio_filestrget.c.o
    libc_tinystdio_filestrputalloc.c.o
    libc_tinystdio_filestrput.c.o
    libc_tinystdio_filewstrget.c.o
    libc_tinystdio_flockfile.c.o
    libc_tinystdio_flockfile_init.c.o
    libc_tinystdio_fmemopen.c.o
    libc_tinystdio_fopen.c.o
    libc_tinystdio_fprintf.c.o
    libc_tinystdio_fputc.c.o
    libc_tinystdio_fputs.c.o
    libc_tinystdio_fputwc.c.o
    libc_tinystdio_fputws.c.o
    libc_tinystdio_fread.c.o
    libc_tinystdio_freopen.c.o
    libc_tinystdio_fscanf.c.o
    libc_tinystdio_fseek.c.o
    libc_tinystdio_fseeko.c.o
    libc_tinystdio_fsetpos.c.o
    libc_tinystdio_ftell.c.o
    libc_tinystdio_ftello.c.o
    libc_tinystdio_ftoa_ryu.c.o
    libc_tinystdio_ftox_engine.c.o
    libc_tinystdio_ftrylockfile.c.o
    libc_tinystdio_funlockfile.c.o
    libc_tinystdio_funopen.c.o
    libc_tinystdio_fwide.c.o
    libc_tinystdio_fwprintf.c.o
    libc_tinystdio_fwrite.c.o
    libc_tinystdio_fwscanf.c.o
    libc_tinystdio_gcvt.c.o
    libc_tinystdio_gcvtf.c.o
    libc_tinystdio_gcvtl.c.o
    libc_tinystdio_getchar.c.o
    libc_tinystdio_getdelim.c.o
    libc_tinystdio_getline.c.o
    libc_tinystdio_gets.c.o
    libc_tinystdio_getwchar.c.o
    libc_tinystdio_ldtoa_engine.c.o
    libc_tinystdio_ldtox_engine.c.o
    libc_tinystdio_matchcaseprefix.c.o
    libc_tinystdio_mktemp.c.o
    libc_tinystdio_perror.c.o
    libc_tinystdio_posixiob_stderr.c.o
    libc_tinystdio_posixiob_stdin.c.o
    libc_tinystdio_posixiob_stdout.c.o
    libc_tinystdio_printf.c.o
    libc_tinystdio_putchar.c.o
    libc_tinystdio_puts.c.o
    libc_tinystdio_putwchar.c.o
    libc_tinystdio_remove.c.o
    libc_tinystdio_rewind.c.o
    libc_tinystdio_ryu_divpow2.c.o
    libc_tinystdio_ryu_log10.c.o
    libc_tinystdio_ryu_log2pow5.c.o
    libc_tinystdio_ryu_pow5bits.c.o
    libc_tinystdio_ryu_table.c.o
    libc_tinystdio_ryu_umul128.c.o
    libc_tinystdio_scanf.c.o
    libc_tinystdio_setbuf.c.o
    libc_tinystdio_setbuffer.c.o
    libc_tinystdio_setlinebuf.c.o
    libc_tinystdio_setvbuf.c.o
    libc_tinystdio_sflags.c.o
    libc_tinystdio_snprintf.c.o
    libc_tinystdio_snprintfd.c.o
    libc_tinystdio_snprintff.c.o
    libc_tinystdio_sprintf.c.o
    libc_tinystdio_sprintfd.c.o
    libc_tinystdio_sprintff.c.o
    libc_tinystdio_sprintf_s.c.o
    libc_tinystdio_sscanf.c.o
    libc_tinystdio_strfromd.c.o
    libc_tinystdio_strfromf.c.o
    libc_tinystdio_strfroml.c.o
    libc_tinystdio_strtod.c.o
    libc_tinystdio_strtod_l.c.o
    libc_tinystdio_strtof.c.o
    libc_tinystdio_strtof_l.c.o
    libc_tinystdio_strtoimax.c.o
    libc_tinystdio_strtoimax_l.c.o
    libc_tinystdio_strtol.c.o
    libc_tinystdio_strtold.c.o
    libc_tinystdio_strtold_l.c.o
    libc_tinystdio_strtol_l.c.o
    libc_tinystdio_strtoll.c.o
    libc_tinystdio_strtoll_l.c.o
    libc_tinystdio_strtoul.c.o
    libc_tinystdio_strtoul_l.c.o
    libc_tinystdio_strtoull.c.o
    libc_tinystdio_strtoull_l.c.o
    libc_tinystdio_strtoumax.c.o
    libc_tinystdio_strtoumax_l.c.o
    libc_tinystdio_swprintf.c.o
    libc_tinystdio_swscanf.c.o
    libc_tinystdio_tmpfile.c.o
    libc_tinystdio_tmpnam.c.o
    libc_tinystdio_ungetc.c.o
    libc_tinystdio_ungetwc.c.o
    libc_tinystdio_vasnprintf.c.o
    libc_tinystdio_vasprintf.c.o
    libc_tinystdio_vffprintf.c.o
    libc_tinystdio_vffscanf.c.o
    libc_tinystdio_vfiprintf.c.o
    libc_tinystdio_vfiscanf.c.o
    libc_tinystdio_vflprintf.c.o
    libc_tinystdio_vflscanf.c.o
    libc_tinystdio_vfmprintf.c.o
    libc_tinystdio_vfmscanf.c.o
    libc_tinystdio_vfprintf.c.o
    libc_tinystdio_vfprintf_s.c.o
    libc_tinystdio_vfscanf.c.o
    libc_tinystdio_vfwprintf.c.o
    libc_tinystdio_vfwscanf.c.o
    libc_tinystdio_vprintf.c.o
    libc_tinystdio_vscanf.c.o
    libc_tinystdio_vsnprintf.c.o
    libc_tinystdio_vsnprintf_s.c.o
    libc_tinystdio_vsprintf.c.o
    libc_tinystdio_vsscanf.c.o
    libc_tinystdio_vswprintf.c.o
    libc_tinystdio_vswscanf.c.o
    libc_tinystdio_vwprintf.c.o
    libc_tinystdio_vwscanf.c.o
    libc_tinystdio_wcstod.c.o
    libc_tinystdio_wcstod_l.c.o
    libc_tinystdio_wcstof.c.o
    libc_tinystdio_wcstof_l.c.o
    libc_tinystdio_wcstoimax.c.o
    libc_tinystdio_wcstoimax_l.c.o
    libc_tinystdio_wcstol.c.o
    libc_tinystdio_wcstold.c.o
    libc_tinystdio_wcstold_l.c.o
    libc_tinystdio_wcstol_l.c.o
    libc_tinystdio_wcstoll.c.o
    libc_tinystdio_wcstoll_l.c.o
    libc_tinystdio_wcstoul.c.o
    libc_tinystdio_wcstoul_l.c.o
    libc_tinystdio_wcstoull.c.o
    libc_tinystdio_wcstoull_l.c.o
    libc_tinystdio_wcstoumax.c.o
    libc_tinystdio_wcstoumax_l.c.o
    libc_tinystdio_wprintf.c.o
    libc_tinystdio_wscanf.c.o
    # libc_ubsan_ubsan_cfi_type_check_to_string.c.o
    # libc_ubsan_ubsan_error.c.o
    # libc_ubsan_ubsan_handle_add_overflow.c.o
    # libc_ubsan_ubsan_handle_alignment_assumption.c.o
    # libc_ubsan_ubsan_handle_builtin_unreachable.c.o
    # libc_ubsan_ubsan_handle_cfi_bad_type.c.o
    # libc_ubsan_ubsan_handle_cfi_check_fail.c.o
    # libc_ubsan_ubsan_handle_divrem_overflow.c.o
    # libc_ubsan_ubsan_handle_dynamic_type_cache_miss.c.o
    # libc_ubsan_ubsan_handle_float_cast_overflow.c.o
    # libc_ubsan_ubsan_handle_function_type_mismatch.c.o
    # libc_ubsan_ubsan_handle_implicit_conversion.c.o
    # libc_ubsan_ubsan_handle_invalid_builtin.c.o
    # libc_ubsan_ubsan_handle_invalid_objc_cast.c.o
    # libc_ubsan_ubsan_handle_load_invalid_value.c.o
    # libc_ubsan_ubsan_handle_missing_return.c.o
    # libc_ubsan_ubsan_handle_mul_overflow.c.o
    # libc_ubsan_ubsan_handle_negate_overflow.c.o
    # libc_ubsan_ubsan_handle_nonnull_arg.c.o
    # libc_ubsan_ubsan_handle_nonnull_return.c.o
    # libc_ubsan_ubsan_handle_nonnull_return_v1.c.o
    # libc_ubsan_ubsan_handle_nullability_arg.c.o
    # libc_ubsan_ubsan_handle_nullability_return.c.o
    # libc_ubsan_ubsan_handle_nullability_return_v1.c.o
    # libc_ubsan_ubsan_handle_out_of_bounds.c.o
    # libc_ubsan_ubsan_handle_pointer_overflow.c.o
    # libc_ubsan_ubsan_handle_shift_out_of_bounds.c.o
    # libc_ubsan_ubsan_handle_sub_overflow.c.o
    # libc_ubsan_ubsan_handle_type_mismatch.c.o
    # libc_ubsan_ubsan_handle_type_mismatch_v1.c.o
    # libc_ubsan_ubsan_handle_vla_bound_not_positive.c.o
    # libc_ubsan_ubsan_message.c.o
    # libc_ubsan_ubsan_type_check_to_string.c.o
    # libc_ubsan_ubsan_val_to_imax.c.o
    # libc_ubsan_ubsan_val_to_string.c.o
    # libc_ubsan_ubsan_val_to_umax.c.o
    # libc_ubsan_ubsan_warning.c.o
    # libc_uchar_c16rtomb.c.o
    # libc_uchar_c32rtomb.c.o
    # libc_uchar_c8rtomb.c.o
    # libc_uchar_mbrtoc16.c.o
    # libc_uchar_mbrtoc32.c.o
    # libc_uchar_mbrtoc8.c.o
    # libc_xdr_xdr_array.c.o
    # libc_xdr_xdr.c.o
    # libc_xdr_xdr_float.c.o
    # libc_xdr_xdr_mem.c.o
    # libc_xdr_xdr_private.c.o
    # libc_xdr_xdr_rec.c.o
    # libc_xdr_xdr_reference.c.o
    # libc_xdr_xdr_sizeof.c.o
    # libc_xdr_xdr_stdio.c.o
    # libm_common_copysignl.c.o
    # libm_common_exp10l.c.o
    # libm_common_exp_data.c.o
    # libm_common_fabsl.c.o
    # libm_common_frexpl.c.o
    # libm_common_log2_data.c.o
    # libm_common_log_data.c.o
    # libm_common_math_denorm.c.o
    # libm_common_math_denormf.c.o
    # libm_common_math_denorml.c.o
    # libm_common_math_err_check_oflow.c.o
    # libm_common_math_err_check_uflow.c.o
    # libm_common_math_err_divzero.c.o
    # libm_common_math_errf_check_oflowf.c.o
    # libm_common_math_errf_check_uflowf.c.o
    # libm_common_math_errf_divzerof.c.o
    # libm_common_math_errf_invalidf.c.o
    # libm_common_math_errf_may_uflowf.c.o
    # libm_common_math_errf_oflowf.c.o
    # libm_common_math_errf_uflowf.c.o
    # libm_common_math_errf_with_errnof.c.o
    # libm_common_math_err_invalid.c.o
    # libm_common_math_err_may_uflow.c.o
    # libm_common_math_err_oflow.c.o
    # libm_common_math_err_uflow.c.o
    # libm_common_math_err_with_errno.c.o
    # libm_common_math_inexact.c.o
    # libm_common_math_inexactf.c.o
    # libm_common_math_inexactl.c.o
    # libm_common_nanl.c.o
    # libm_common_nexttoward.c.o
    # libm_common_nexttowardf.c.o
    # libm_common_pow_log_data.c.o
    # libm_common_s_cbrt.c.o
    # libm_common_s_copysign.c.o
    # libm_common_s_exp10.c.o
    # libm_common_s_expm1.c.o
    # libm_common_sf_cbrt.c.o
    # libm_common_sf_copysign.c.o
    # libm_common_s_fdim.c.o
    # libm_common_sf_exp10.c.o
    # libm_common_sf_exp2_data.c.o
    # libm_common_sf_expm1.c.o
    # libm_common_sf_fdim.c.o
    # libm_common_sf_finite.c.o
    # libm_common_sf_fma.c.o
    # libm_common_sf_fmax.c.o
    # libm_common_sf_fmin.c.o
    # libm_common_sf_fpclassify.c.o
    # libm_common_sf_getpayload.c.o
    # libm_common_sf_ilogb.c.o
    # libm_common_sf_infinity.c.o
    # libm_common_s_finite.c.o
    # libm_common_sf_iseqsig.c.o
    # libm_common_sf_isinf.c.o
    # libm_common_sf_isnan.c.o
    # libm_common_sf_issignaling.c.o
    # libm_common_sf_llrint.c.o
    # libm_common_sf_llround.c.o
    # libm_common_sf_log1p.c.o
    # libm_common_sf_log2_data.c.o
    # libm_common_sf_logb.c.o
    # libm_common_sf_log_data.c.o
    # libm_common_sf_lrint.c.o
    # libm_common_sf_lround.c.o
    # libm_common_s_fma.c.o
    # libm_common_s_fmax.c.o
    # libm_common_s_fmin.c.o
    # libm_common_sf_modf.c.o
    # libm_common_sf_nan.c.o
    # libm_common_sf_nextafter.c.o
    # libm_common_s_fpclassify.c.o
    # libm_common_sf_pow10.c.o
    # libm_common_sf_pow_log2_data.c.o
    # libm_common_sf_remquo.c.o
    # libm_common_sf_scalbln.c.o
    # libm_common_sf_scalbn.c.o
    # libm_common_s_getpayload.c.o
    # libm_common_signgam.c.o
    # libm_common_s_ilogb.c.o
    # libm_common_sincosf_data.c.o
    # libm_common_s_infinity.c.o
    # libm_common_s_iseqsig.c.o
    # libm_common_s_isinf.c.o
    # libm_common_s_isnan.c.o
    # libm_common_s_issignaling.c.o
    # libm_common_sl_iseqsig.c.o
    # libm_common_sl_issignaling.c.o
    # libm_common_s_llrint.c.o
    # libm_common_s_llround.c.o
    # libm_common_s_log1p.c.o
    # libm_common_s_log2.c.o
    # libm_common_s_logb.c.o
    # libm_common_s_lrint.c.o
    # libm_common_s_lround.c.o
    # libm_common_s_modf.c.o
    # libm_common_s_nan.c.o
    # libm_common_s_nextafter.c.o
    # libm_common_s_pow10.c.o
    # libm_common_s_remquo.c.o
    # libm_common_s_scalbln.c.o
    # libm_common_s_scalbn.c.o
    # libm_common_s_signbit.c.o
    # libm_complex_cabs.c.o
    # libm_complex_cabsf.c.o
    # libm_complex_cabsl.c.o
    # libm_complex_cacos.c.o
    # libm_complex_cacosf.c.o
    # libm_complex_cacosh.c.o
    # libm_complex_cacoshf.c.o
    # libm_complex_cacoshl.c.o
    # libm_complex_cacosl.c.o
    # libm_complex_carg.c.o
    # libm_complex_cargf.c.o
    # libm_complex_cargl.c.o
    # libm_complex_casin.c.o
    # libm_complex_casinf.c.o
    # libm_complex_casinh.c.o
    # libm_complex_casinhf.c.o
    # libm_complex_casinhl.c.o
    # libm_complex_casinl.c.o
    # libm_complex_catan.c.o
    # libm_complex_catanf.c.o
    # libm_complex_catanh.c.o
    # libm_complex_catanhf.c.o
    # libm_complex_catanhl.c.o
    # libm_complex_catanl.c.o
    # libm_complex_ccos.c.o
    # libm_complex_ccosf.c.o
    # libm_complex_ccosh.c.o
    # libm_complex_ccoshf.c.o
    # libm_complex_ccoshl.c.o
    # libm_complex_ccosl.c.o
    # libm_complex_cephes_subr.c.o
    # libm_complex_cephes_subrf.c.o
    # libm_complex_cephes_subrl.c.o
    # libm_complex_cexp.c.o
    # libm_complex_cexpf.c.o
    # libm_complex_cexpl.c.o
    # libm_complex_cimag.c.o
    # libm_complex_cimagf.c.o
    # libm_complex_cimagl.c.o
    # libm_complex_clog10.c.o
    # libm_complex_clog10f.c.o
    # libm_complex_clog10l.c.o
    # libm_complex_clog.c.o
    # libm_complex_clogf.c.o
    # libm_complex_clogl.c.o
    # libm_complex_conj.c.o
    # libm_complex_conjf.c.o
    # libm_complex_conjl.c.o
    # libm_complex_cpow.c.o
    # libm_complex_cpowf.c.o
    # libm_complex_cpowl.c.o
    # libm_complex_cproj.c.o
    # libm_complex_cprojf.c.o
    # libm_complex_cprojl.c.o
    # libm_complex_creal.c.o
    # libm_complex_crealf.c.o
    # libm_complex_creall.c.o
    # libm_complex_csin.c.o
    # libm_complex_csinf.c.o
    # libm_complex_csinh.c.o
    # libm_complex_csinhf.c.o
    # libm_complex_csinhl.c.o
    # libm_complex_csinl.c.o
    # libm_complex_csqrt.c.o
    # libm_complex_csqrtf.c.o
    # libm_complex_csqrtl.c.o
    # libm_complex_ctan.c.o
    # libm_complex_ctanf.c.o
    # libm_complex_ctanh.c.o
    # libm_complex_ctanhf.c.o
    # libm_complex_ctanhl.c.o
    # libm_complex_ctanl.c.o
    # libm_fenv_fe_dfl_env.c.o
    # libm_fenv_fegetmode.c.o
    # libm_fenv_fenv.c.o
    # libm_fenv_fesetmode.c.o
    # libm_ld_e_acoshl.c.o
    # libm_ld_e_acosl.c.o
    # libm_ld_e_asinl.c.o
    # libm_ld_e_atan2l.c.o
    # libm_ld_e_atanhl.c.o
    # libm_ld_e_coshl.c.o
    # libm_ld_e_expl.c.o
    # libm_ld_e_fmodl.c.o
    # libm_ld_e_hypotl.c.o
    # libm_ld_e_lgammal.c.o
    # libm_ld_e_lgammal_r.c.o
    # libm_ld_e_log10l.c.o
    # libm_ld_e_log2l.c.o
    # libm_ld_e_logl.c.o
    # libm_ld_e_powl.c.o
    # libm_ld_e_remainderl.c.o
    # libm_ld_e_sinhl.c.o
    # libm_ld_e_sqrtl.c.o
    # libm_ld_e_tgammal.c.o
    # libm_ld_invtrig.c.o
    # libm_ld_k_cosl.c.o
    # libm_ld_k_rem_pio2.c.o
    # libm_ld_k_sinl.c.o
    # libm_ld_k_tanl.c.o
    # libm_ld_math_errl_check_oflowl.c.o
    # libm_ld_math_errl_check_uflowl.c.o
    # libm_ld_math_errl_divzerol.c.o
    # libm_ld_math_errl_invalidl.c.o
    # libm_ld_math_errl_oflowl.c.o
    # libm_ld_math_errl_uflowl.c.o
    # libm_ld_math_errl_with_errnol.c.o
    # libm_ld_polevll.c.o
    # libm_ld_s_asinhl.c.o
    # libm_ld_s_atanl.c.o
    # libm_ld_s_cbrtl.c.o
    # libm_ld_s_ceill.c.o
    # libm_ld_s_copysignl.c.o
    # libm_ld_s_cosl.c.o
    # libm_ld_s_erfl.c.o
    # libm_ld_s_exp2l.c.o
    # libm_ld_s_expm1l.c.o
    # libm_ld_s_fabsl.c.o
    # libm_ld_s_fdiml.c.o
    # libm_ld_s_finitel.c.o
    # libm_ld_s_floorl.c.o
    # libm_ld_s_fmal.c.o
    # libm_ld_s_fmaxl.c.o
    # libm_ld_s_fminl.c.o
    # libm_ld_s_fpclassifyl.c.o
    # libm_ld_s_frexpl.c.o
    # libm_ld_s_getpayloadl.c.o
    # libm_ld_s_ilogbl.c.o
    # libm_ld_s_isinfl.c.o
    # libm_ld_s_isnanl.c.o
    # libm_ld_s_issignalingl.c.o
    # libm_ld_s_llrintl.c.o
    # libm_ld_s_llroundl.c.o
    # libm_ld_s_log1pl.c.o
    # libm_ld_s_logbl.c.o
    # libm_ld_s_lrintl.c.o
    # libm_ld_s_lroundl.c.o
    # libm_ld_s_modfl.c.o
    # libm_ld_s_nanl.c.o
    # libm_ld_s_nextafterl.c.o
    # libm_ld_s_nexttoward.c.o
    # libm_ld_s_nexttowardf.c.o
    # libm_ld_s_remquol.c.o
    # libm_ld_s_rintl.c.o
    # libm_ld_s_roundl.c.o
    # libm_ld_s_scalbl.c.o
    # libm_ld_s_scalbln.c.o
    # libm_ld_s_scalbnl.c.o
    # libm_ld_s_signbitl.c.o
    # libm_ld_s_significandl.c.o
    # libm_ld_s_sincosl.c.o
    # libm_ld_s_sinl.c.o
    # libm_ld_s_tanhl.c.o
    # libm_ld_s_tanl.c.o
    # libm_ld_s_truncl.c.o
    # libm_machine_arm_s_ceil.c.o
    # libm_machine_arm_s_fabs.c.o
    # libm_machine_arm_sf_ceil.c.o
    # libm_machine_arm_sf_fabs.c.o
    # libm_machine_arm_sf_floor.c.o
    # libm_machine_arm_sf_fma_arm.c.o
    # libm_machine_arm_s_floor.c.o
    # libm_machine_arm_s_fma_arm.c.o
    # libm_machine_arm_sf_nearbyint.c.o
    # libm_machine_arm_sf_rint.c.o
    # libm_machine_arm_sf_round.c.o
    # libm_machine_arm_sf_sqrt.c.o
    # libm_machine_arm_sf_trunc.c.o
    # libm_machine_arm_s_nearbyint.c.o
    # libm_machine_arm_s_rint.c.o
    # libm_machine_arm_s_round.c.o
    # libm_machine_arm_s_sqrt.c.o
    # libm_machine_arm_s_trunc.c.o
    # libm_math_k_cos.c.o
    # libm_math_kf_cos.c.o
    # libm_math_kf_rem_pio2.c.o
    # libm_math_kf_sin.c.o
    # libm_math_kf_tan.c.o
    # libm_math_k_rem_pio2.c.o
    # libm_math_k_sin.c.o
    # libm_math_k_tan.c.o
    # libm_math_s_acos.c.o
    # libm_math_s_acosh.c.o
    # libm_math_s_asin.c.o
    # libm_math_s_asinh.c.o
    # libm_math_s_atan2.c.o
    # libm_math_s_atan.c.o
    # libm_math_s_atanh.c.o
    # libm_math_s_cos.c.o
    # libm_math_s_cosh.c.o
    # libm_math_s_drem.c.o
    # libm_math_s_erf.c.o
    # libm_math_s_exp2.c.o
    # libm_math_s_exp.c.o
    # libm_math_sf_acos.c.o
    # libm_math_sf_acosh.c.o
    # libm_math_sf_asin.c.o
    # libm_math_sf_asinh.c.o
    # libm_math_sf_atan2.c.o
    # libm_math_sf_atan.c.o
    # libm_math_sf_atanh.c.o
    # libm_math_sf_cos.c.o
    # libm_math_sf_cosh.c.o
    # libm_math_sf_drem.c.o
    # libm_math_sf_erf.c.o
    # libm_math_sf_exp2.c.o
    # libm_math_sf_exp.c.o
    # libm_math_sf_fmod.c.o
    # libm_math_sf_frexp.c.o
    # libm_math_sf_hypot.c.o
    # libm_math_sf_j0.c.o
    # libm_math_sf_j1.c.o
    # libm_math_sf_jn.c.o
    # libm_math_sf_lgamma.c.o
    # libm_math_sf_log10.c.o
    # libm_math_sf_log2.c.o
    # libm_math_sf_log.c.o
    # libm_math_s_fmod.c.o
    # libm_math_sf_pow.c.o
    # libm_math_sf_remainder.c.o
    # libm_math_sf_rem_pio2.c.o
    # libm_math_s_frexp.c.o
    # libm_math_sf_scalb.c.o
    # libm_math_sf_signif.c.o
    # libm_math_sf_sin.c.o
    # libm_math_sf_sincos.c.o
    # libm_math_sf_sinh.c.o
    # libm_math_sf_tan.c.o
    # libm_math_sf_tanh.c.o
    # libm_math_sf_tgamma.c.o
    # libm_math_s_hypot.c.o
    # libm_math_s_j0.c.o
    # libm_math_s_j1.c.o
    # libm_math_s_jn.c.o
    # libm_math_s_lgamma.c.o
    # libm_math_sl_hypot.c.o
    # libm_math_s_log10.c.o
    # libm_math_s_log.c.o
    # libm_math_s_pow.c.o
    # libm_math_s_remainder.c.o
    # libm_math_s_rem_pio2.c.o
    # libm_math_srf_lgamma.c.o
    # libm_math_sr_lgamma.c.o
    # libm_math_s_scalb.c.o
    # libm_math_s_signif.c.o
    # libm_math_s_sin.c.o
    # libm_math_s_sincos.c.o
    # libm_math_s_sinh.c.o
    # libm_math_s_tan.c.o
    # libm_math_s_tanh.c.o
    # libm_math_s_tgamma.c.o
    memchr.c.o
    memchr.S.o
    memcpy.c.o
    memcpy.S.o
    memmove.c.o
    memset.c.o
    memset.S.o
    nano-calloc.c.o
    nano-free.c.o
    nano-getpagesize.c.o
    nano-mallinfo.c.o
    nano-malloc.c.o
    nano-malloc-stats.c.o
    nano-malloc-usable-size.c.o
    nano-mallopt.c.o
    nano-memalign.c.o
    nano-posix-memalign.c.o
    nano-pvalloc.c.o
    nano-realloc.c.o
    nano-valloc.c.o
    setjmp.S.o
    strcmp.S.o
    strcpy.S.o
    strlen.c.o
    strlen.S.o
)
