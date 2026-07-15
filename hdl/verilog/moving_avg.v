// k_filter :: Moving average (power-of-2 boxcar). Bit-exact with
// sim/fixed_models.moving_avg_hw: running sum of the last 2**LOG2N samples,
// divided by an arithmetic right shift. Fills from zero (hardware warm-up).
`default_nettype none
module moving_avg #(
    parameter integer WIDTH = 16,
    parameter integer LOG2N = 3           // window = 2**LOG2N (LOG2N >= 1)
) (
    input  wire                    clk,
    input  wire                    rst,
    input  wire                    in_valid,
    input  wire signed [WIDTH-1:0] x,
    output reg  signed [WIDTH-1:0] y,
    output reg                     out_valid
);
    localparam integer N = (1 <<< LOG2N);

    reg  signed [WIDTH-1:0]        sr [0:N-1];       // sample shift register
    reg  signed [WIDTH+LOG2N:0]    sum;              // running sum (WIDTH+LOG2N+1 bits)
    reg         [LOG2N-1:0]        idx;              // wraps mod N
    integer i;

    wire signed [WIDTH+LOG2N:0]    sum_next = sum - sr[idx] + x;
    wire signed [WIDTH+LOG2N:0]    avg      = sum_next >>> LOG2N;   // floor divide by N

    always @(posedge clk) begin
        if (rst) begin
            sum       <= 0;
            idx       <= 0;
            y         <= 0;
            out_valid <= 1'b0;
            for (i = 0; i < N; i = i + 1) sr[i] <= 0;
        end else if (in_valid) begin
            sum        <= sum_next;
            sr[idx]    <= x;
            idx        <= idx + 1'b1;      // LOG2N-bit counter wraps mod N
            y          <= avg[WIDTH-1:0];
            out_valid  <= 1'b1;
        end else begin
            out_valid  <= 1'b0;
        end
    end
endmodule
`default_nettype wire
