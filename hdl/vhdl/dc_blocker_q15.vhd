-- k_filter :: DC blocker / 1st-order high-pass, Q15 pole.
-- Bit-exact with sim/fixed_models.dc_blocker_q15:
--   y = x - x_prev + (R*y_prev) >> 15 (arithmetic), saturated. Seed on first sample.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity dc_blocker_q15 is
    generic (
        WIDTH : integer := 16;
        R     : integer := 32604          -- Q15 pole, ~0.995
    );
    port (
        clk       : in  std_logic;
        rst       : in  std_logic;
        in_valid  : in  std_logic;
        x         : in  signed(WIDTH-1 downto 0);
        y         : out signed(WIDTH-1 downto 0);
        out_valid : out std_logic
    );
end entity;

architecture rtl of dc_blocker_q15 is
    signal yr      : signed(WIDTH-1 downto 0) := (others => '0');
    signal x_prev  : signed(WIDTH-1 downto 0) := (others => '0');
    signal started : std_logic := '0';
    constant HI : integer := 2**(WIDTH-1) - 1;
    constant LO : integer := -(2**(WIDTH-1));
begin
    y <= yr;

    process(clk)
        variable fb   : signed(WIDTH+18 downto 0);
        variable ynew : signed(WIDTH+18 downto 0);
    begin
        if rising_edge(clk) then
            if rst = '1' then
                yr <= (others => '0');
                x_prev <= (others => '0');
                started <= '0';
                out_valid <= '0';
            elsif in_valid = '1' then
                if started = '0' then
                    x_prev <= x;
                    yr <= (others => '0');    -- first output 0
                    started <= '1';
                else
                    fb   := shift_right(resize(to_signed(R, 18) * resize(yr, WIDTH+1),
                                               fb'length), 15);
                    ynew := resize(x, ynew'length) - resize(x_prev, ynew'length) + fb;
                    if ynew > HI then
                        yr <= to_signed(HI, WIDTH);
                    elsif ynew < LO then
                        yr <= to_signed(LO, WIDTH);
                    else
                        yr <= resize(ynew, WIDTH);
                    end if;
                    x_prev <= x;
                end if;
                out_valid <= '1';
            else
                out_valid <= '0';
            end if;
        end if;
    end process;
end architecture;
