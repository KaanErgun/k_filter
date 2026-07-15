-- k_filter :: Deadband / hysteresis hold. Bit-exact with sim/fixed_models.deadband.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity deadband is
    generic (WIDTH : integer := 16; THRESHOLD : integer := 500);
    port (
        clk, rst, in_valid : in  std_logic;
        x                  : in  signed(WIDTH-1 downto 0);
        y                  : out signed(WIDTH-1 downto 0);
        out_valid          : out std_logic
    );
end entity;

architecture rtl of deadband is
    signal yr      : signed(WIDTH-1 downto 0) := (others => '0');
    signal started : std_logic := '0';
begin
    y <= yr;
    process(clk)
        variable d  : signed(WIDTH downto 0);
        variable ad : signed(WIDTH downto 0);
    begin
        if rising_edge(clk) then
            if rst = '1' then
                yr <= (others => '0'); started <= '0'; out_valid <= '0';
            elsif in_valid = '1' then
                if started = '0' then
                    yr <= x; started <= '1';
                else
                    d := resize(x, WIDTH+1) - resize(yr, WIDTH+1);
                    if d < 0 then ad := -d; else ad := d; end if;
                    if ad > THRESHOLD then
                        yr <= x;
                    end if;
                end if;
                out_valid <= '1';
            else
                out_valid <= '0';
            end if;
        end if;
    end process;
end architecture;
