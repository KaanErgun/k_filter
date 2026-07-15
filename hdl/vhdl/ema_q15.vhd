-- k_filter :: EMA / 1st-order low-pass, Q15 coefficient.
-- Synchronous, synthesizable. Bit-exact with sim/fixed_models.ema_q15 and the C
-- ema_fixed: y += (ALPHA*(x-y)) >> 15 (arithmetic), saturated, seed on first sample.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity ema_q15 is
    generic (
        WIDTH : integer := 16;
        ALPHA : integer := 3277           -- Q15 coefficient, 0..32767
    );
    port (
        clk       : in  std_logic;
        rst       : in  std_logic;        -- synchronous, active high
        in_valid  : in  std_logic;
        x         : in  signed(WIDTH-1 downto 0);
        y         : out signed(WIDTH-1 downto 0);
        out_valid : out std_logic
    );
end entity;

architecture rtl of ema_q15 is
    signal yr      : signed(WIDTH-1 downto 0) := (others => '0');
    signal started : std_logic := '0';
    constant HI : integer := 2**(WIDTH-1) - 1;
    constant LO : integer := -(2**(WIDTH-1));
begin
    y <= yr;

    process(clk)
        variable diff : signed(WIDTH downto 0);
        variable prod : signed(WIDTH+18 downto 0);
        variable inc  : signed(WIDTH+18 downto 0);
        variable ynew : signed(WIDTH+18 downto 0);
    begin
        if rising_edge(clk) then
            if rst = '1' then
                yr <= (others => '0');
                started <= '0';
                out_valid <= '0';
            elsif in_valid = '1' then
                if started = '0' then
                    yr <= x;               -- seed on first sample
                    started <= '1';
                else
                    diff := resize(x, WIDTH+1) - resize(yr, WIDTH+1);
                    prod := resize(to_signed(ALPHA, 18) * diff, prod'length);
                    inc  := shift_right(prod, 15);          -- arithmetic (floor)
                    ynew := resize(yr, ynew'length) + inc;
                    if ynew > HI then
                        yr <= to_signed(HI, WIDTH);
                    elsif ynew < LO then
                        yr <= to_signed(LO, WIDTH);
                    else
                        yr <= resize(ynew, WIDTH);
                    end if;
                end if;
                out_valid <= '1';
            else
                out_valid <= '0';
            end if;
        end if;
    end process;
end architecture;
