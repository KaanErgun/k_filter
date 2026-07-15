// k_filter :: Deadband / hysteresis hold. Bit-exact with sim/fixed_models.deadband.
// Holds the last output until |x - y| > THRESHOLD, then snaps to x. Seed on first.
`default_nettype none
module deadband #(
    parameter integer WIDTH     = 16,
    parameter integer THRESHOLD = 500
) (
    input  wire                    clk,
    input  wire                    rst,
    input  wire                    in_valid,
    input  wire signed [WIDTH-1:0] x,
    output reg  signed [WIDTH-1:0] y,
    output reg                     out_valid
);
    localparam signed [WIDTH:0] TH = THRESHOLD;

    reg started;
    wire signed [WIDTH:0] d  = x - y;
    wire signed [WIDTH:0] ad = (d < 0) ? -d : d;

    always @(posedge clk) begin
        if (rst) begin
            y <= {WIDTH{1'b0}};
            started <= 1'b0;
            out_valid <= 1'b0;
        end else if (in_valid) begin
            if (!started) begin
                y <= x;
                started <= 1'b1;
            end else if (ad > TH) begin
                y <= x;
            end
            out_valid <= 1'b1;
        end else begin
            out_valid <= 1'b0;
        end
    end
endmodule
`default_nettype wire
