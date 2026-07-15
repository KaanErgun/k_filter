// k_filter :: EMA / 1st-order low-pass, Q15 coefficient.
// Synchronous, synthesizable. Bit-exact with sim/fixed_models.ema_q15 and the C
// ema_fixed: y += (ALPHA*(x-y)) >>> 15, saturated to WIDTH bits, seed on first sample.
`default_nettype none
module ema_q15 #(
    parameter integer WIDTH = 16,
    parameter integer ALPHA = 3277        // Q15 coefficient, 0..32767
) (
    input  wire                    clk,
    input  wire                    rst,     // synchronous, active high
    input  wire                    in_valid,
    input  wire signed [WIDTH-1:0] x,
    output reg  signed [WIDTH-1:0] y,
    output reg                     out_valid
);
    localparam signed [16:0]        ALPHA_S = ALPHA;   // 0..32767 as positive signed
    localparam signed [WIDTH:0]     HI = (1 <<< (WIDTH-1)) - 1;
    localparam signed [WIDTH:0]     LO = -(1 <<< (WIDTH-1));

    reg started;
    wire signed [WIDTH:0]       diff = x - y;               // WIDTH+1 bits
    wire signed [WIDTH+17:0]    prod = ALPHA_S * diff;      // wide product
    wire signed [WIDTH+17:0]    inc  = prod >>> 15;         // arithmetic shift (floor)
    wire signed [WIDTH+17:0]    ynew = y + inc;

    always @(posedge clk) begin
        if (rst) begin
            y         <= {WIDTH{1'b0}};
            started   <= 1'b0;
            out_valid <= 1'b0;
        end else if (in_valid) begin
            if (!started) begin
                y       <= x;                 // seed on first sample
                started <= 1'b1;
            end else if (ynew > HI) begin
                y <= HI[WIDTH-1:0];
            end else if (ynew < LO) begin
                y <= LO[WIDTH-1:0];
            end else begin
                y <= ynew[WIDTH-1:0];
            end
            out_valid <= 1'b1;
        end else begin
            out_valid <= 1'b0;
        end
    end
endmodule
`default_nettype wire
