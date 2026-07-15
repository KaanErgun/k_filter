// k_filter :: Slew-rate limiter. Bit-exact with sim/fixed_models.slew_limiter.
// Bounds |output change| per sample to MAX_STEP, saturated. Seed on first sample.
`default_nettype none
module slew_limiter #(
    parameter integer WIDTH    = 16,
    parameter integer MAX_STEP = 2000
) (
    input  wire                    clk,
    input  wire                    rst,
    input  wire                    in_valid,
    input  wire signed [WIDTH-1:0] x,
    output reg  signed [WIDTH-1:0] y,
    output reg                     out_valid
);
    localparam signed [WIDTH:0] HI = (1 <<< (WIDTH-1)) - 1;
    localparam signed [WIDTH:0] LO = -(1 <<< (WIDTH-1));
    localparam signed [WIDTH:0] MS = MAX_STEP;

    reg started;
    wire signed [WIDTH:0]   d      = x - y;
    wire signed [WIDTH:0]   dclamp = (d > MS) ? MS : (d < -MS) ? -MS : d;
    wire signed [WIDTH+1:0] ynew   = y + dclamp;

    always @(posedge clk) begin
        if (rst) begin
            y <= {WIDTH{1'b0}};
            started <= 1'b0;
            out_valid <= 1'b0;
        end else if (in_valid) begin
            if (!started) begin
                y <= x;
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
