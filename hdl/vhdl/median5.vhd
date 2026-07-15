-- k_filter :: Median of a fixed 5-sample window. Bit-exact with
-- sim/fixed_models.median5. A bubble sort in a function selects the middle.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity median5 is
    generic (WIDTH : integer := 16);
    port (
        clk, rst, in_valid : in  std_logic;
        x                  : in  signed(WIDTH-1 downto 0);
        y                  : out signed(WIDTH-1 downto 0);
        out_valid          : out std_logic
    );
end entity;

architecture rtl of median5 is
    signal w0, w1, w2, w3 : signed(WIDTH-1 downto 0) := (others => '0');

    type arr_t is array(0 to 4) of signed(WIDTH-1 downto 0);

    function med5(a, b, c, d, e : signed(WIDTH-1 downto 0)) return signed is
        variable s : arr_t;
        variable t : signed(WIDTH-1 downto 0);
    begin
        s := (a, b, c, d, e);
        for i in 0 to 3 loop
            for j in 0 to 3 - i loop
                if s(j) > s(j+1) then
                    t := s(j); s(j) := s(j+1); s(j+1) := t;
                end if;
            end loop;
        end loop;
        return s(2);
    end function;
begin
    process(clk)
    begin
        if rising_edge(clk) then
            if rst = '1' then
                w0 <= (others => '0'); w1 <= (others => '0');
                w2 <= (others => '0'); w3 <= (others => '0');
                y <= (others => '0'); out_valid <= '0';
            elsif in_valid = '1' then
                y  <= med5(x, w0, w1, w2, w3);
                w0 <= x; w1 <= w0; w2 <= w1; w3 <= w2;
                out_valid <= '1';
            else
                out_valid <= '0';
            end if;
        end if;
    end process;
end architecture;
