
val runtime = io.Source.fromFile("example1.ptx").getLines
.toArray

runtime
.takeWhile(!_.startsWith(".visible"))
.foreach(println)

val compiled = io.Source.fromFile("input.ptx").getLines
.toArray

compiled
.dropWhile(!_.startsWith(".func"))
.foreach(println)

runtime
.dropWhile(!_.startsWith(".visible"))
.map( line => if (line.contains("_Z8mapitfooiPiS_,")) { "\tmapit," } else { line })
.foreach(println)

