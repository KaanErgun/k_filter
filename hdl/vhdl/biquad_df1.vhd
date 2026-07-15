-- k_filter :: Biquad, Direct Form I (fixed Q12 low-pass). Bit-exact with
-- sim/fixed_models.biquad_df1.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity biquad_df1 is
    generic (WIDTH : integer := 16);
    port (
        clk, rst, in_valid : in  std_logic;
        x                  : in  signed(WIDTH-1 downto 0);
        y                  : out signed(WIDTH-1 downto 0);
        out_valid          : out std_logic
    );
end entity;

architecture rtl of biquad_df1 is
    signal x1, x2, y1, y2 : signed(WIDTH-1 downto 0) := (others => '0');
    constant B0 : integer := 276;
    constant B1 : integer := 553;
    constant B2 : integer := 276;
    constant A1 : integer := -4682;
    constant A2 : integer := 1691;
    constant QSHIFT : integer := 12;
    constant HI : integer := 2**(WIDTH-1) - 1;
    constant LO : integer := -(2**(WIDTH-1));

    function mac(coef : integer; sample : signed) return signed is
    begin
        return to_signed(coef, 16) * resize(sample, WIDTH+1);
    end function;
begin
    process(clk)
        variable acc : signed(WIDTH+20 downto 0);
        variable yv  : signed(WIDTH+20 downto 0);
        variable yo  : signed(WIDTH-1 downto 0);
    begin
        if rising_edge(clk) then
            if rst = '1' then
                x1 <= (others => '0'); x2 <= (others => '0');
                y1 <= (others => '0'); y2 <= (others => '0');
                y <= (others => '0'); out_valid <= '0';
            elsif in_valid = '1' then
                acc := resize(mac(B0, x),  acc'length)
                     + resize(mac(B1, x1), acc'length)
                     + resize(mac(B2, x2), acc'length)
                     - resize(mac(A1, y1), acc'length)
                     - resize(mac(A2, y2), acc'length);
                yv := shift_right(acc, QSHIFT);
                if yv > HI then
                    yo := to_signed(HI, WIDTH);
                elsif yv < LO then
                    yo := to_signed(LO, WIDTH);
                else
                    yo := resize(yv, WIDTH);
                end if;
                x2 <= x1; x1 <= x;
                y2 <= y1; y1 <= yo;
                y  <= yo;
                out_valid <= '1';
            else
                out_valid <= '0';
            end if;
        end if;
    end process;
end architecture;
