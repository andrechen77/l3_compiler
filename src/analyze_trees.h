#pragma once

namespace L3::program::analyze {
	void generate_data_flow(Program &program); // TODO;
	// basically take the completed program and generate computation trees
	// for each instruction, then update all the basic blocks to have proper
	// in and out sets

	void merge_trees(BasicBlock &block); // TODO
	// assumes that data flow has already been generated
	// merges trees whenever possible
	// probably the way to do this is to start at the last tree, keeping a
	// running map of variables to the most recently encountered tree that
	// reads from that variable and could merge on it. if you encounter
	// an eligible candidate that writes to a variable, then merge those trees

	void merge_trees(Program &program); // TODO
	// assumes that data flow has already been generated for the program;
	// merges trees in all the basic blocks
}