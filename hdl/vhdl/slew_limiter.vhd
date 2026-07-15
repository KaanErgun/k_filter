-- k_filter :: Slew-rate limiter. Bit-exact with sim/fixed_models.slew_limiter.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity slew_limiter is
    generic (WIDTH : integer := 16; MAX_STEP : integer := 2000);
    port (
        clk, rst, in_valid : in  std_logic;
        x                  : in  signed(WIDTH-1 downto 0);
        y                  : out signed(WIDTH-1 downto 0);
        out_valid          : out std_logic
    );
end entity;

architecture rtl of slew_limiter is
    signal yr      : signed(WIDTH-1 downto 0) := (others => '0');
    signal started : std_logic := '0';
    constant HI : integer := 2**(WIDTH-1) - 1;
    constant LO : integer := -(2**(WIDTH-1));
begin
    y <= yr;
    process(clk)
        variable d    : signed(WIDTH downto 0);
        variable dc   : signed(WIDTH downto 0);
        variable ynew : signed(WIDTH+1 downto 0);
    begin
        if rising_edge(clk) then
            if rst = '1' then
                yr <= (others => '0'); started <= '0'; out_valid <= '0';
            elsif in_valid = '1' then
                if started = '0' then
                    yr <= x; started <= '1';
                else
                    d := resize(x, WIDTH+1) - resize(yr, WIDTH+1);
                    if d > MAX_STEP then
                        dc := to_signed(MAX_STEP, WIDTH+1);
                    elsif d < -MAX_STEP then
                        dc := to_signed(-MAX_STEP, WIDTH+1);
                    else
                        dc := d;
                    end if;
                    ynew := resize(yr, WIDTH+2) + resize(dc, WIDTH+2);
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
