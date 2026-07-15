// k_filter :: DC blocker / 1st-order high-pass, Q15 pole. Bit-exact with
// sim/fixed_models.dc_blocker_q15: y = x - x_prev + (R*y_prev) >>> 15, saturated.
// Seeds on the first sample (first output is 0).
`default_nettype none
module dc_blocker_q15 #(
    parameter integer WIDTH = 16,
    parameter integer R     = 32604       // Q15 pole, ~0.995
) (
    input  wire                    clk,
    input  wire                    rst,
    input  wire                    in_valid,
    input  wire signed [WIDTH-1:0] x,
    output reg  signed [WIDTH-1:0] y,
    output reg                     out_valid
);
    localparam signed [16:0]     R_S = R;
    localparam signed [WIDTH:0]  HI  = (1 <<< (WIDTH-1)) - 1;
    localparam signed [WIDTH:0]  LO  = -(1 <<< (WIDTH-1));

    reg  signed [WIDTH-1:0]      x_prev;
    reg                          started;
    wire signed [WIDTH+17:0]     fb   = (R_S * y) >>> 15;         // feedback term
    wire signed [WIDTH+17:0]     ynew = x - x_prev + fb;

    always @(posedge clk) begin
        if (rst) begin
            x_prev    <= {WIDTH{1'b0}};
            y         <= {WIDTH{1'b0}};
            started   <= 1'b0;
            out_valid <= 1'b0;
        end else if (in_valid) begin
            if (!started) begin
                x_prev  <= x;
                y       <= {WIDTH{1'b0}};   // first output 0
                started <= 1'b1;
            end else begin
                if (ynew > HI)      y <= HI[WIDTH-1:0];
                else if (ynew < LO) y <= LO[WIDTH-1:0];
                else                y <= ynew[WIDTH-1:0];
                x_prev <= x;
            end
            out_valid <= 1'b1;
        end else begin
            out_valid <= 1'b0;
        end
    end
endmodule
`default_nettype wire
