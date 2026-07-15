// k_filter :: Biquad, 2nd-order IIR, Direct Form I (fixed Q12 low-pass coeffs).
// Bit-exact with sim/fixed_models.biquad_df1:
//   acc = B0*x + B1*x1 + B2*x2 - A1*y1 - A2*y2;  y = sat(acc >>> 12).
`default_nettype none
module biquad_df1 #(
    parameter integer WIDTH = 16
) (
    input  wire                    clk,
    input  wire                    rst,
    input  wire                    in_valid,
    input  wire signed [WIDTH-1:0] x,
    output reg  signed [WIDTH-1:0] y,
    output reg                     out_valid
);
    // RBJ low-pass fc/fs=0.1, Q=0.707, quantized to Q12.
    localparam signed [15:0] B0 = 276, B1 = 553, B2 = 276, A1 = -4682, A2 = 1691;
    localparam integer       QSHIFT = 12;
    localparam signed [WIDTH:0] HI = (1 <<< (WIDTH-1)) - 1;
    localparam signed [WIDTH:0] LO = -(1 <<< (WIDTH-1));

    reg signed [WIDTH-1:0] x1, x2, y1, y2;

    wire signed [WIDTH+19:0] acc = B0*x + B1*x1 + B2*x2 - A1*y1 - A2*y2;
    wire signed [WIDTH+19:0] yv  = acc >>> QSHIFT;
    wire signed [WIDTH-1:0]  yo  = (yv > HI) ? HI[WIDTH-1:0] :
                                   (yv < LO) ? LO[WIDTH-1:0] : yv[WIDTH-1:0];

    always @(posedge clk) begin
        if (rst) begin
            x1 <= 0; x2 <= 0; y1 <= 0; y2 <= 0;
            y  <= 0;
            out_valid <= 1'b0;
        end else if (in_valid) begin
            x2 <= x1; x1 <= x;
            y2 <= y1; y1 <= yo;
            y  <= yo;
            out_valid <= 1'b1;
        end else begin
            out_valid <= 1'b0;
        end
    end
endmodule
`default_nettype wire
