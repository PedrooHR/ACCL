# /*******************************************************************************
#  Copyright (C) 2021 Xilinx, Inc
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
# *******************************************************************************/

DEVICE=xcu250-figd2104-2L-e
IPNAME=stream_segmenter
SEGMENTER_IP=build_$(IPNAME)/sol1/impl/ip/xilinx_com_hls_$(IPNAME)_1_0.zip
TARGET=ip

all: $(SEGMENTER_IP) 

$(SEGMENTER_IP): ../build.tcl $(IPNAME).cpp
	vitis_hls $< -tclargs $(TARGET) $(DEVICE) $(IPNAME)

