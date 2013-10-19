/* 
GPiler - glink.java
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

import java.lang.*;
import java.util.*;
import java.io.*;

public class glink {

	ArrayList<String> Readfile(String filename){
		ArrayList<String> ret = new ArrayList<String>();
		try{
			BufferedReader br = new BufferedReader(new FileReader(filename));
			String line;
			while ((line = br.readLine()) != null) {
				ret.add(line);
			}
			br.close();
		}catch(Exception e){}
		return ret;
	}

	void run() {
		ArrayList<String> runtime = Readfile("example1.ptx");
		ArrayList<String> compiled = Readfile("input.ptx");		

		for(String line : runtime){
			if(line.startsWith(".visible"))
				break;
			System.out.println(line);
		}

		boolean running = false;
		for(String line : compiled){
			if(running){
				System.out.println(line);
				continue;
			}
			
			if(line.startsWith(".func")){
				System.out.println(line);
				running=true;
			}
		}

		running=false;
		for(String line : runtime){
			if(running){
				if(line.contains("_Z8mapitfooiPiS_,")){
					System.out.println("\tmapit,");
				}else 
					System.out.println(line);
				continue;
			}
			
			if(line.startsWith(".visible")){
				System.out.println(line);
				running=true;
			}
		}
	}

   	public static void main(String[] args) {
    		glink gl = new glink(); 
		gl.run();
   	}
};
