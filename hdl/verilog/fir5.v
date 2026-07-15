// k_filter :: 5-tap FIR (fixed Q15 low-pass coefficients).
// Bit-exact with sim/fixed_models.fir5: y = (C0*x + C1*t0 + .. + C4*t3) >>> 15,
// coefficients sum to 32768 (unity DC gain). Taps fill from zero.
`default_nettype none
module fir5 #(
    parameter integer WIDTH = 16
) (
    input  wire                    clk,
    input  wire                    rst,
    input  wire                    in_valid,
    input  wire signed [WIDTH-1:0] x,
    output reg  signed [WIDTH-1:0] y,
    output reg                     out_valid
);
    localparam signed [15:0] C0 = 2048, C1 = 8192, C2 = 12288, C3 = 8192, C4 = 2048;
    localparam integer       QSHIFT = 15;
    localparam signed [WIDTH:0] HI = (1 <<< (WIDTH-1)) - 1;
    localparam signed [WIDTH:0] LO = -(1 <<< (WIDTH-1));

    reg signed [WIDTH-1:0] t0, t1, t2, t3;   // previous samples (t0 = most recent past)

    // Current window (newest first): {x, t0, t1, t2, t3}
    wire signed [WIDTH+18:0] acc = C0*x + C1*t0 + C2*t1 + C3*t2 + C4*t3;
    wire signed [WIDTH+18:0] yv  = acc >>> QSHIFT;

    always @(posedge clk) begin
        if (rst) begin
            t0 <= 0; t1 <= 0; t2 <= 0; t3 <= 0;
            y  <= 0;
            out_valid <= 1'b0;
        end else if (in_valid) begin
            t0 <= x; t1 <= t0; t2 <= t1; t3 <= t2;   // shift
            if (yv > HI)      y <= HI[WIDTH-1:0];
            else if (yv < LO) y <= LO[WIDTH-1:0];
            else              y <= yv[WIDTH-1:0];
            out_valid <= 1'b1;
        end else begin
            out_valid <= 1'b0;
        end
    end
endmodule
`default_nettype wire
