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
    private final int boardSize;

    public Room(int id, String name, int currentPlayers, int maxPlayers, String state, int boardSize) {
        this.id = id;
        this.name = name;
        this.currentPlayers = currentPlayers;
        this.maxPlayers = maxPlayers;
        this.state = state;
        this.boardSize = boardSize;
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

    public int getBoardSize() {
        return boardSize;
    }

    @Override
    public String toString() {
        return String.format("Room #%d: %s (%d/%d) - %dx%d - %s",
                           id, name, currentPlayers, maxPlayers, boardSize, boardSize, state);
    }
}
