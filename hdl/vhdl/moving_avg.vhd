-- k_filter :: Moving average (power-of-2 boxcar).
-- Bit-exact with sim/fixed_models.moving_avg_hw: running sum of the last 2**LOG2N
-- samples divided by an arithmetic right shift. Fills from zero.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity moving_avg is
    generic (
        WIDTH : integer := 16;
        LOG2N : integer := 3              -- window = 2**LOG2N (LOG2N >= 1)
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

architecture rtl of moving_avg is
    constant N : integer := 2**LOG2N;
    type sr_t is array(0 to N-1) of signed(WIDTH-1 downto 0);
    signal sr  : sr_t := (others => (others => '0'));
    signal sum : signed(WIDTH+LOG2N downto 0) := (others => '0');
    signal idx : integer range 0 to N-1 := 0;
begin
    process(clk)
        variable sum_next : signed(WIDTH+LOG2N downto 0);
        variable avg      : signed(WIDTH+LOG2N downto 0);
    begin
        if rising_edge(clk) then
            if rst = '1' then
                sum <= (others => '0');
                idx <= 0;
                y   <= (others => '0');
                sr  <= (others => (others => '0'));
                out_valid <= '0';
            elsif in_valid = '1' then
                sum_next := sum - resize(sr(idx), sum'length) + resize(x, sum'length);
                avg      := shift_right(sum_next, LOG2N);   -- floor divide by N
                sum <= sum_next;
                sr(idx) <= x;
                if idx = N-1 then idx <= 0; else idx <= idx + 1; end if;
                y   <= resize(avg, WIDTH);
                out_valid <= '1';
            else
                out_valid <= '0';
            end if;
        end if;
    end process;
end architecture;
