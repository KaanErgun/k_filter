-- k_filter :: 5-tap FIR (fixed Q15 low-pass). Bit-exact with sim/fixed_models.fir5.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity fir5 is
    generic (WIDTH : integer := 16);
    port (
        clk, rst, in_valid : in  std_logic;
        x                  : in  signed(WIDTH-1 downto 0);
        y                  : out signed(WIDTH-1 downto 0);
        out_valid          : out std_logic
    );
end entity;

architecture rtl of fir5 is
    signal t0, t1, t2, t3 : signed(WIDTH-1 downto 0) := (others => '0');
    constant C0 : integer := 2048;
    constant C1 : integer := 8192;
    constant C2 : integer := 12288;
    constant C3 : integer := 8192;
    constant C4 : integer := 2048;
    constant QSHIFT : integer := 15;
    constant HI : integer := 2**(WIDTH-1) - 1;
    constant LO : integer := -(2**(WIDTH-1));

    function mac(coef : integer; sample : signed) return signed is
    begin
        return to_signed(coef, 16) * resize(sample, WIDTH+1);
    end function;
begin
    process(clk)
        variable acc : signed(WIDTH+19 downto 0);
        variable yv  : signed(WIDTH+19 downto 0);
    begin
        if rising_edge(clk) then
            if rst = '1' then
                t0 <= (others => '0'); t1 <= (others => '0');
                t2 <= (others => '0'); t3 <= (others => '0');
                y <= (others => '0'); out_valid <= '0';
            elsif in_valid = '1' then
                acc := resize(mac(C0, x),  acc'length)
                     + resize(mac(C1, t0), acc'length)
                     + resize(mac(C2, t1), acc'length)
                     + resize(mac(C3, t2), acc'length)
                     + resize(mac(C4, t3), acc'length);
                yv := shift_right(acc, QSHIFT);
                t0 <= x; t1 <= t0; t2 <= t1; t3 <= t2;
                if yv > HI then
                    y <= to_signed(HI, WIDTH);
                elsif yv < LO then
                    y <= to_signed(LO, WIDTH);
                else
                    y <= resize(yv, WIDTH);
                end if;
                out_valid <= '1';
            else
                out_valid <= '0';
            end if;
        end if;
    end process;
end architecture;
