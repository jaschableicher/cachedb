
//write some stuff to the database, save then write some more
//then crash it without it beeing saved to the snapshot dump
// Restart the app which then must reload oldest snapshot + replay the rest of the aof file it has