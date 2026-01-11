package cz.zcu.kiv.ups.pexeso;

import cz.zcu.kiv.ups.pexeso.controller.LoginController;
import cz.zcu.kiv.ups.pexeso.util.Logger;
import javafx.application.Application;
import javafx.fxml.FXMLLoader;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.stage.Stage;

/**
 * Main entry point for the Pexeso client application
 */
public class Main extends Application {

    @Override
    public void start(Stage primaryStage) throws Exception {
        // Initialize logger
        Logger.init();
        Logger.info("Application starting...");
        // Load FXML
        FXMLLoader loader = new FXMLLoader(getClass().getResource("/cz/zcu/kiv/ups/pexeso/ui/LoginView.fxml"));
        Parent root = loader.load();

        // Get controller and set stage
        LoginController controller = loader.getController();
        controller.setStage(primaryStage);

        // Setup scene
        Scene scene = new Scene(root, 600, 500);

        // Setup stage
        primaryStage.setTitle("Pexeso Client - Login");
        primaryStage.setScene(scene);
        primaryStage.setResizable(true);
        primaryStage.show();

        // Handle application close
        primaryStage.setOnCloseRequest(event -> {
            Logger.info("Application closing...");
            Logger.close();
            System.exit(0);
        });
    }

    public static void main(String[] args) {
        launch(args);
    }
}
