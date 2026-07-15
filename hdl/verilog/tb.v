// Generic testbench: streams stimulus.txt (one signed decimal per line) through
// the selected DUT and prints each output sample as a decimal on its own line.
// Select the filter at compile time, e.g. iverilog -DF_EMA tb.v ema_q15.v.
`timescale 1ns/1ps
`default_nettype none
module tb;
    localparam integer WIDTH = 16;

    reg                     clk = 1'b0;
    reg                     rst = 1'b1;
    reg                     in_valid = 1'b0;
    reg  signed [WIDTH-1:0] x = 0;
    wire signed [WIDTH-1:0] y;
    wire                    out_valid;

`ifdef F_EMA
    ema_q15       #(.WIDTH(WIDTH), .ALPHA(3277))  dut (
        .clk(clk), .rst(rst), .in_valid(in_valid), .x(x), .y(y), .out_valid(out_valid));
`elsif F_MAVG
    moving_avg    #(.WIDTH(WIDTH), .LOG2N(3))     dut (
        .clk(clk), .rst(rst), .in_valid(in_valid), .x(x), .y(y), .out_valid(out_valid));
`elsif F_DC
    dc_blocker_q15 #(.WIDTH(WIDTH), .R(32604))    dut (
        .clk(clk), .rst(rst), .in_valid(in_valid), .x(x), .y(y), .out_valid(out_valid));
`endif

    always #5 clk = ~clk;

    integer fd, val, r;
    initial begin
        @(negedge clk);
        @(negedge clk);
        rst = 1'b0;
        fd = $fopen("stimulus.txt", "r");
        if (fd == 0) begin
            $display("ERROR: cannot open stimulus.txt");
            $finish;
        end
        r = $fscanf(fd, "%d\n", val);
        while (r == 1) begin
            @(negedge clk);
            x = val[WIDTH-1:0];
            in_valid = 1'b1;
            @(posedge clk);      // DUT registers y on this edge
            #1;
            if (out_valid) $display("%0d", y);
            r = $fscanf(fd, "%d\n", val);
        end
        $fclose(fd);
        $finish;
    end
endmodule
`default_nettype wire
