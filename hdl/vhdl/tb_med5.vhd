-- Testbench for median5: streams stimulus.txt, prints outputs. ghdl --std=08.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use std.textio.all;
use std.env.all;

entity tb_med5 is
end entity;

architecture sim of tb_med5 is
    constant WIDTH : integer := 16;
    signal clk : std_logic := '0';
    signal rst : std_logic := '1';
    signal in_valid : std_logic := '0';
    signal x : signed(WIDTH-1 downto 0) := (others => '0');
    signal y : signed(WIDTH-1 downto 0);
    signal out_valid : std_logic;
begin
    dut : entity work.median5
        generic map (WIDTH => WIDTH)
        port map (clk => clk, rst => rst, in_valid => in_valid, x => x, y => y,
                  out_valid => out_valid);
    clk <= not clk after 5 ns;
    process
        file f : text; variable l, ol : line; variable v : integer;
        variable st : file_open_status;
    begin
        wait until falling_edge(clk); wait until falling_edge(clk);
        rst <= '0';
        file_open(st, f, "stimulus.txt", read_mode);
        assert st = open_ok report "cannot open stimulus.txt" severity failure;
        while not endfile(f) loop
            readline(f, l); read(l, v);
            wait until falling_edge(clk);
            x <= to_signed(v, WIDTH); in_valid <= '1';
            wait until rising_edge(clk); wait for 1 ns;
            if out_valid = '1' then
                write(ol, integer'image(to_integer(y))); writeline(output, ol);
            end if;
        end loop;
        file_close(f); finish;
    end process;
end architecture;
