// k_filter :: Median of a fixed 5-sample window. Bit-exact with
// sim/fixed_models.median5. A small bubble sort (in a function) selects the
// middle order statistic. Window fills from zero.
`default_nettype none
module median5 #(
    parameter integer WIDTH = 16
) (
    input  wire                    clk,
    input  wire                    rst,
    input  wire                    in_valid,
    input  wire signed [WIDTH-1:0] x,
    output reg  signed [WIDTH-1:0] y,
    output reg                     out_valid
);
    reg signed [WIDTH-1:0] w0, w1, w2, w3;   // previous 4 samples

    function signed [WIDTH-1:0] med5;
        input signed [WIDTH-1:0] a, b, c, d, e;
        reg signed [WIDTH-1:0] s [0:4];
        reg signed [WIDTH-1:0] t;
        integer i, j;
        begin
            s[0] = a; s[1] = b; s[2] = c; s[3] = d; s[4] = e;
            for (i = 0; i < 4; i = i + 1)
                for (j = 0; j < 4 - i; j = j + 1)
                    if (s[j] > s[j+1]) begin
                        t = s[j]; s[j] = s[j+1]; s[j+1] = t;
                    end
            med5 = s[2];
        end
    endfunction

    always @(posedge clk) begin
        if (rst) begin
            w0 <= 0; w1 <= 0; w2 <= 0; w3 <= 0;
            y  <= 0;
            out_valid <= 1'b0;
        end else if (in_valid) begin
            y  <= med5(x, w0, w1, w2, w3);
            w0 <= x; w1 <= w0; w2 <= w1; w3 <= w2;   // shift
            out_valid <= 1'b1;
        end else begin
            out_valid <= 1'b0;
        end
    end
endmodule
`default_nettype wire
