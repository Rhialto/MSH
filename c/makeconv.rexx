/*
 * MakeConv.rexx
 *
 * Make a conversion table file for MSH:
 *
 * This code is (C) Copyright 1992 by Olaf Seibert. All rights reserved.
 * May not be used or copied without a licence.
 */

parse arg file rest

if (file = "") | (file = "?") then do
    say "Usage: [rx] makeconv filename {amiga-char other-char}"
    exit 10
end

call init()

do while rest ~= ""
    parse value rest with amiga other rest
    call convert(amiga, other)
end

call finish()
exit 0

init:
    drop to.
    drop from.

    do i = 0 to 255
	to.i = i
	from.i = i
    end
    return

convert:
    amiga = normalise(arg(1))
    other = normalise(arg(2))

    to.amiga = other
    from.other = amiga

    say "Amiga char" amiga "("d2c(amiga)") =" other"."

    return

normalise:
    value = arg(1)

    if left(value, 1) = "$" then do
	value = x2d(substr(value, 2))
    end
    else if left(value, 2) = "0x" then do
	value = x2d(substr(value, 3))
    end
    else if left(value, 1) = "'" | left(value, 1) = '"' then do
	value = c2d(substr(value, 2, 1))
    end

    return value

finish:
    if (open(fp, file, "w") = 0) then do
	say "Cannot open" file "for write."
	exit 10
    end

    to = ""
    from = ""
    do i = 0 to 255
	to = to || d2c(to.i)
	from = from || d2c(from.i)
    end
    writech(fp, to)
    writech(fp, from)

    call close(fp)

    return
