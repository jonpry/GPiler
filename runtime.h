/* 
GPiler - runtime.h
Copyright (C) 2013 Jon Pry and Charles Cooper

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "node.h"

class RuntimeInst{
public:
	RuntimeInst(NFunctionDeclaration* func);
	void print();

	string name;
	VariableList inputs,outputs;
};


class Runtime {
public:
	Runtime() {}
	void AddFunction(NFunctionDeclaration *func) {runtimes[func->id->name] = new RuntimeInst(func);}
	void print();
	map<string,RuntimeInst*> runtimes;
};

void generate_runtime(NFunctionDeclaration* target, FunctionList *modules);

