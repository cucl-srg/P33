/*******************************************************************************
 *
 *  NetFPGA-10G http://www.netfpga.org
 *
 *  File:
 *        nf10_output_port_lookup.v
 *
 *  Library:
 *        hw/std/pcores/nf10_router_output_port_lookup_v1_00_a
 *
 *  Module:
 *        nf10_output_port_lookup
 *
 *  Author:
 *        Adam Covington, Gianni Antichi
 *
 *  Description:
 *        Hardwire the hardware interfaces to CPU and vice versa
 *
 *  Copyright notice:
 *        Copyright (C) 2010, 2011 The Board of Trustees of The Leland Stanford
 *                                 Junior University
 *
 *  Licence:
 *        This file is part of the NetFPGA 10G development base package.
 *
 *        This file is free code: you can redistribute it and/or modify it under
 *        the terms of the GNU Lesser General Public License version 2.1 as
 *        published by the Free Software Foundation.
 *
 *        This package is distributed in the hope that it will be useful, but
 *        WITHOUT ANY WARRANTY; without even the implied warranty of
 *        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *        Lesser General Public License for more details.
 *
 *        You should have received a copy of the GNU Lesser General Public
 *        License along with the NetFPGA source package.  If not, see
 *        http://www.gnu.org/licenses/.
 *
 */

module nf10_router_output_port_lookup
#(
    parameter C_FAMILY = "virtex5",
    parameter C_S_AXI_DATA_WIDTH = 32,
    parameter C_S_AXI_ADDR_WIDTH = 32,
    parameter C_USE_WSTRB = 0,
    parameter C_DPHASE_TIMEOUT = 0,
    parameter C_S_AXI_ACLK_FREQ_HZ = 100,
    parameter C_BASEADDR = 32'h76800000,
    parameter C_HIGHADDR = 32'h7680FFFF,
    //Master AXI Stream Data Width
    parameter C_M_AXIS_DATA_WIDTH=256,
    parameter C_S_AXIS_DATA_WIDTH=256,
    parameter C_M_AXIS_TUSER_WIDTH=128,
    parameter C_S_AXIS_TUSER_WIDTH=128,
    parameter SRC_PORT_POS=16,
    parameter DST_PORT_POS=24
)
(
    // Global Ports
    input 				AXI_ACLK,
    input 				AXI_RESETN,

    // Master Stream Ports (interface to data path)
    output [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA,
    output [((C_M_AXIS_DATA_WIDTH/8))-1:0]M_AXIS_TSTRB,
    output reg [C_M_AXIS_TUSER_WIDTH-1:0]M_AXIS_TUSER,
    output 				M_AXIS_TVALID,
    input  				M_AXIS_TREADY,
    output 				M_AXIS_TLAST,

    // Slave Stream Ports (interface to RX queues)
    input [C_S_AXIS_DATA_WIDTH-1:0] 	S_AXIS_TDATA,
    input [((C_S_AXIS_DATA_WIDTH/8))-1:0]S_AXIS_TSTRB,
    input [C_S_AXIS_TUSER_WIDTH-1:0] 	S_AXIS_TUSER,
    input  				S_AXIS_TVALID,
    output 				S_AXIS_TREADY,
    input  				S_AXIS_TLAST,

    // register port definitions

    input [C_S_AXI_ADDR_WIDTH-1:0]	S_AXI_AWADDR,
    input 				S_AXI_AWVALID,
    input [C_S_AXI_DATA_WIDTH-1:0] 	S_AXI_WDATA,
    input [C_S_AXI_DATA_WIDTH/8-1:0] 	S_AXI_WSTRB,
    input 				S_AXI_WVALID,
    input 				S_AXI_BREADY,
    input [C_S_AXI_ADDR_WIDTH-1:0] 	S_AXI_ARADDR,
    input 				S_AXI_ARVALID,
    input 				S_AXI_RREADY,
    output 				S_AXI_ARREADY,
    output [C_S_AXI_DATA_WIDTH-1:0] 	S_AXI_RDATA,
    output [1:0] 			S_AXI_RRESP,
    output 				S_AXI_RVALID,
    output 				S_AXI_WREADY,
    output [1:0] 			S_AXI_BRESP,
    output 				S_AXI_BVALID,
    output 				S_AXI_AWREADY

);

   function integer log2;
      input integer number;
      begin
         log2=0;
         while(2**log2<number) begin
            log2=log2+1;
         end
      end
   endfunction // log2

   // ------------ Internal Params --------
   localparam MODULE_HEADER = 0;
   localparam IN_PACKET     = 1;

   //------------- Wires ------------------
   wire  [C_M_AXIS_TUSER_WIDTH-1:0] tuser_fifo;
   reg 			  state, state_next;

   // ------------ Modules ----------------

   fallthrough_small_fifo
        #( .WIDTH(C_M_AXIS_DATA_WIDTH+C_M_AXIS_TUSER_WIDTH+C_M_AXIS_DATA_WIDTH/8+1),
           .MAX_DEPTH_BITS(2))
      input_fifo
        (// Outputs
         .dout                           ({M_AXIS_TLAST, tuser_fifo, M_AXIS_TSTRB, M_AXIS_TDATA}),
         .full                           (),
         .nearly_full                    (in_fifo_nearly_full),
         .prog_full                      (),
         .empty                          (in_fifo_empty),
         // Inputs
         .din                            ({S_AXIS_TLAST, S_AXIS_TUSER, S_AXIS_TSTRB, S_AXIS_TDATA}),
         .wr_en                          (S_AXIS_TVALID & ~in_fifo_nearly_full),
         .rd_en                          (in_fifo_rd_en),
         .reset                          (~AXI_RESETN),
         .clk                            (AXI_ACLK));

   // ------------- Logic ----------------

   assign S_AXIS_TREADY = !in_fifo_nearly_full;

   // packet is from the cpu if it is on an odd numbered port
   assign pkt_is_from_cpu = M_AXIS_TUSER[SRC_PORT_POS+1] ||
			    M_AXIS_TUSER[SRC_PORT_POS+3] ||
			    M_AXIS_TUSER[SRC_PORT_POS+5] ||
			    M_AXIS_TUSER[SRC_PORT_POS+7];

   // modify the dst port in tuser
   always @(*) begin
      M_AXIS_TUSER = tuser_fifo;
      state_next      = state;

      case(state)
	MODULE_HEADER: begin
	   if (M_AXIS_TVALID) begin

               // Send all packets to ethernet port 1 (nf1)
		M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b100;

               /* Here's how we'd implement a NIC: */
               /*
	       if(pkt_is_from_cpu)
	           M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = {1'b0,
			tuser_fifo[SRC_PORT_POS+7:SRC_PORT_POS+1]};
	       else
		   M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = {
			tuser_fifo[SRC_PORT_POS+6:SRC_PORT_POS], 1'b0};
	       */
	       if(M_AXIS_TREADY)
			    state_next = IN_PACKET;
	   end
	end // case: MODULE_HEADER

	IN_PACKET: begin
	   if(M_AXIS_TLAST & M_AXIS_TVALID & M_AXIS_TREADY) begin
	      state_next = MODULE_HEADER;
	   end
	end
      endcase // case (state)
   end // always @ (*)

   always @(posedge AXI_ACLK) begin
      if(~AXI_RESETN) begin
	 state <= MODULE_HEADER;
      end
      else begin
	 state <= state_next;
      end
   end

   // Handle output
   assign in_fifo_rd_en = M_AXIS_TREADY && !in_fifo_empty;
   assign M_AXIS_TVALID = !in_fifo_empty;

endmodule // output_port_lookup
