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
