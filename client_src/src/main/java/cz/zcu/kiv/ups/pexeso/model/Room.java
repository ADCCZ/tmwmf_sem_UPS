package cz.zcu.kiv.ups.pexeso.model;

/**
 * Model representing a game room
 */
public class Room {
    private final int id;
    private final String name;
    private final int currentPlayers;
    private final int maxPlayers;
    private final String state;

    public Room(int id, String name, int currentPlayers, int maxPlayers, String state) {
        this.id = id;
        this.name = name;
        this.currentPlayers = currentPlayers;
        this.maxPlayers = maxPlayers;
        this.state = state;
    }

    public int getId() {
        return id;
    }

    public String getName() {
        return name;
    }

    public int getCurrentPlayers() {
        return currentPlayers;
    }

    public int getMaxPlayers() {
        return maxPlayers;
    }

    public String getState() {
        return state;
    }

    @Override
    public String toString() {
        return String.format("%s (%d/%d) - %s", name, currentPlayers, maxPlayers, state);
    }
}
