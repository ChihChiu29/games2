function drop_text_left(c, str, y, o)
	local i = strlen(str)
	local x = 39 - (strlen(str) / 2) + o
	while (i > 0)
	do
		local a = 0
		local time = 0

		if (strbyte(str, i) ~= strbyte(" ", 1)) then
			while (a < x + i - 1)
			do
				Term_putch(a - 1, y, c, 32)
				Term_putch(a, y, c, strbyte(str, i))
				time = time + 1
				if time >= 4 then
					Term_xtra(TERM_XTRA_DELAY, 1)
					time = 0
				end
				Term_redraw_section(a - 1, y, a, y)
				a = a + 1

				inkey_scan = TRUE
				if (inkey() ~= 0) then
					return TRUE
				end
			end
		end

		i = i - 1
	end
	return FALSE
end

function drop_text_right(c, str, y, o)
	local x = 39 - (strlen(str) / 2) + o
	local i = 1
	while (i <= strlen(str))
	do
		local a = 79
		local time = 0

		if (strbyte(str, i) ~= strbyte(" ", 1)) then
			while (a >= x + i - 1)
			do
				Term_putch(a + 1, y, c, 32)
				Term_putch(a, y, c, strbyte(str, i))
				time = time + 1
				if time >= 4 then
					Term_xtra(TERM_XTRA_DELAY, 1)
					time = 0
				end
				Term_redraw_section(a, y, a + 1, y)
				a = a - 1

				inkey_scan = TRUE
				if (inkey() ~= 0) then
					return TRUE
				end
			end
		end

		i = i + 1
	end
	return FALSE
end

function tome_intro()
	msg_print("Welcome to ToME! Use the [+]More menu for tutorial & help.")
end

add_hook_script(HOOK_INIT, "tome_intro", "lua_intro_init")
