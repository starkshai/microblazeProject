
`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 2026/09/24 20:33:59
// Design Name: 
// Module Name: ds18b20v1
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


module ds18b20v1(
    input  wire       sys_clk,     // system clock, 50MHz
    input  wire       sys_rst_n,   // reset signal, active low

    inout  wire       dq,          // data bus

    output reg [15:0] data,        // temperature output, *0.0625 convert to degrees celsius
    output reg        sign         // temperature sign bit
);

    ////
    //\\* Parameter and Internal Signal *//
    ////

    // parameter definition
    parameter S_INIT       = 3'd1, // initial state
              S_WR_CMD     = 3'd2, // send Skip ROM & Convert Temperature commands
              S_WAIT       = 3'd3, // wait for temperature conversion to finish
              S_INIT_AGAIN = 3'd4, // re-initialize
              S_RD_CMD     = 3'd5, // send Skip ROM & Read Temperature commands
              S_RD_TEMP    = 3'd6; // read temperature state
    parameter WR_44CC_CMD  = 16'h44cc; // Skip ROM + Convert Temperature command, LSB first
    parameter WR_BECC_CMD  = 16'hbecc; // Skip ROM + Read Temperature command, LSB first
    parameter S_WAIT_MAX   = 750000;   // 750ms

    // register definition
    reg        clk_1us;    // divided clock, one tick per 1us
    reg [4:0]  cnt;        // divider counter
    reg [2:0]  state;      // FSM state
    reg [19:0] cnt_1us;    // microsecond counter
    reg [3:0]  bit_cnt;    // bit counter
    reg [15:0] data_tmp;   // temperature read from DS18B20
    reg        flag_pulse; // presence pulse flag during initialization
    reg        dq_out;     // DQ value driven by FPGA
    reg        dq_en;      // DQ output enable

    ////
    //\\* Main Code *//
    ////

    // When dq_en=1, DQ = dq_out; when dq_en=0, DQ is high-impedance
    assign dq = (dq_en == 1) ? dq_out : 1'bz;

    // cnt: divider counter
    always @(posedge sys_clk or negedge sys_rst_n)
        if(sys_rst_n == 1'b0)
            cnt <= 5'b0;
        else if(cnt == 5'd24)
            cnt <= 5'b0;
        else
            cnt <= cnt + 1'b1;

    // clk_1us: generates a 1us clock
    always @(posedge sys_clk or negedge sys_rst_n)
        if(sys_rst_n == 1'b0)
            clk_1us <= 1'b0;
        else if(cnt == 5'd24)
            clk_1us <= ~clk_1us;
        else
            clk_1us <= clk_1us;

    // cnt_1us: microsecond counter for FSM transitions
    always @(posedge clk_1us or negedge sys_rst_n)
        if(sys_rst_n == 1'b0)
            cnt_1us <= 20'b0;
        else if(((state==S_WR_CMD || state==S_RD_CMD || state==S_RD_TEMP) && cnt_1us==20'd64)
                || ((state==S_INIT || state==S_INIT_AGAIN) && cnt_1us==20'd999)
                || (state==S_WAIT && cnt_1us==S_WAIT_MAX))
            cnt_1us <= 20'b0;
        else
            cnt_1us <= cnt_1us + 1'b1;

    // bit_cnt: incremented per bit written/read, cleared after a full byte
    always @(posedge clk_1us or negedge sys_rst_n)
        if(sys_rst_n == 1'b0)
            bit_cnt <= 4'b0;
        else if((state == S_RD_TEMP || state == S_WR_CMD || state == S_RD_CMD)
                && (cnt_1us == 20'd64 && bit_cnt == 4'd15))
            bit_cnt <= 4'b0;
        else if((state == S_WR_CMD || state == S_RD_CMD || state == S_RD_TEMP)
                && cnt_1us == 20'd64)
            bit_cnt <= bit_cnt + 1'b1;

    // presence pulse flag: initialization succeeds only when the
    // slave presence pulse is detected on the bus
    always @(posedge clk_1us or negedge sys_rst_n)
        if(sys_rst_n == 1'b0)
            flag_pulse <= 1'b0;
        else if(cnt_1us == 20'd570 && dq == 1'b0 && (state == S_INIT || state == S_INIT_AGAIN))
            flag_pulse <= 1'b1;
        else if(cnt_1us == 999)
            flag_pulse <= 1'b0;
        else
            flag_pulse <= flag_pulse;

    // FSM state transition
    always @(posedge clk_1us or negedge sys_rst_n)
        if(sys_rst_n == 1'b0)
            state <= S_INIT;
        else
            case(state)
                // initialization lasts at least 960us
                S_INIT: // jump after presence pulse received and >960us elapsed
                    if(cnt_1us == 20'd999 && flag_pulse == 1'b1)
                        state <= S_WR_CMD;
                    else
                        state <= S_INIT;
                S_WR_CMD: // jump after Skip ROM and Convert commands sent
                    if(bit_cnt == 4'd15 && cnt_1us == 20'd64)
                        state <= S_WAIT;
                    else
                        state <= S_WR_CMD;
                S_WAIT: // jump after 750ms wait
                    if(cnt_1us == S_WAIT_MAX)
                        state <= S_INIT_AGAIN;
                    else
                        state <= S_WAIT;
                S_INIT_AGAIN: // jump after re-initialization
                    if(cnt_1us == 20'd999 && flag_pulse == 1'b1)
                        state <= S_RD_CMD;
                    else
                        state <= S_INIT_AGAIN;
                S_RD_CMD: // jump after Skip ROM and Read commands sent
                    if(bit_cnt == 4'd15 && cnt_1us == 20'd64)
                        state <= S_RD_TEMP;
                    else
                        state <= S_RD_CMD;
                S_RD_TEMP: // jump after 2-byte temperature read
                    if(bit_cnt == 4'd15 && cnt_1us == 20'd64)
                        state <= S_INIT;
                    else
                        state <= S_RD_TEMP;
                default:
                    state <= S_INIT;
            endcase

    // drive DQ timing for each state
    always @(posedge clk_1us or negedge sys_rst_n)
        if(sys_rst_n == 1'b0)
            begin
                dq_out <= 1'b0;
                dq_en  <= 1'b0;
            end
        else
            case(state)
                // initialization: pull low for at least 480us, then release the bus
                S_INIT:
                    if(cnt_1us < 20'd499)
                        begin
                            dq_out <= 1'b0;
                            dq_en  <= 1'b1;
                        end
                    else
                        begin
                            dq_out <= 1'b0;
                            dq_en  <= 1'b0;
                        end
                // each write slot: at least 60us low and at least 1us recovery
                // write 0: keep the bus low for at least 60us
                // write 1: release the bus within 15us after pulling low
                S_WR_CMD:
                    if(cnt_1us > 20'd62)
                        begin
                            dq_out <= 1'b0;
                            dq_en  <= 1'b0;
                        end
                    else if(cnt_1us <= 20'b1)
                        begin
                            dq_out <= 1'b0;
                            dq_en  <= 1'b1;
                        end
                    else if(WR_44CC_CMD[bit_cnt] == 1'b0)
                        begin
                            dq_out <= 1'b0;
                            dq_en  <= 1'b1;
                        end
                    else if(WR_44CC_CMD[bit_cnt] == 1'b1)
                        begin
                            dq_out <= 1'b0;
                            dq_en  <= 1'b0;
                        end
                // for parasitic power mode, pull DQ high after the Convert command
                S_WAIT:
                    begin
                        dq_out <= 1'b1;
                        dq_en  <= 1'b1;
                    end
                // same timing as the first initialization
                S_INIT_AGAIN:
                    if(cnt_1us < 20'd499)
                        begin
                            dq_out <= 1'b0;
                            dq_en  <= 1'b1;
                        end
                    else
                        begin
                            dq_out <= 1'b0;
                            dq_en  <= 1'b0;
                        end
                // same timing as sending Skip ROM and Read commands
                S_RD_CMD:
                    if(cnt_1us > 20'd62)
                        begin
                            dq_out <= 1'b0;
                            dq_en  <= 1'b0;
                        end
                    else if(cnt_1us <= 20'b1)
                        begin
                            dq_out <= 1'b0;
                            dq_en  <= 1'b1;
                        end
                    else if(WR_BECC_CMD[bit_cnt] == 1'b0)
                        begin
                            dq_out <= 1'b0;
                            dq_en  <= 1'b1;
                        end
                    else if(WR_BECC_CMD[bit_cnt] == 1'b1)
                        begin
                            dq_out <= 1'b0;
                            dq_en  <= 1'b0;
                        end
                // pull the bus low for >1us, then release it
                S_RD_TEMP:
                    if(cnt_1us <= 1)
                        begin
                            dq_out <= 1'b0;
                            dq_en  <= 1'b1;
                        end
                    else
                        begin
                            dq_out <= 1'b0;
                            dq_en  <= 1'b0;
                        end
                default: ;
            endcase

    // data_tmp: register the temperature read from the bus
    always @(posedge clk_1us or negedge sys_rst_n)
        if(sys_rst_n == 1'b0)
            data_tmp <= 12'b0;
        // data is valid within 15us after DQ is pulled low
        else if(state == S_RD_TEMP && cnt_1us == 20'd13)
            data_tmp <= {dq, data_tmp[15:1]};
        else
            data_tmp <= data_tmp;

    // sign handling: output temperature (two's complement to magnitude)
    always @(posedge clk_1us or negedge sys_rst_n)
        if(sys_rst_n == 1'b0)
            data <= 20'b0;
        else if(data_tmp[15] == 1'b0 && state == S_RD_TEMP
                && cnt_1us == 20'd60 && bit_cnt == 4'd15)
            data <= data_tmp[14:0];
        else if(data_tmp[15] == 1'b1 && state == S_RD_TEMP
                && cnt_1us == 20'd60 && bit_cnt == 4'd15)
            data <= ~data_tmp[14:0] + 1'b1;

    // sign handling: output sign bit
    always @(posedge clk_1us or negedge sys_rst_n)
        if(sys_rst_n == 1'b0)
            sign <= 1'b0;
        else if(data_tmp[15] == 1'b0 && state == S_RD_TEMP
                && cnt_1us == 20'd60 && bit_cnt == 4'd15)
            sign <= 1'b0;
        else if(data_tmp[15] == 1'b1 && state == S_RD_TEMP
                && cnt_1us == 20'd60 && bit_cnt == 4'd15)
            sign <= 1'b1;

endmodule
