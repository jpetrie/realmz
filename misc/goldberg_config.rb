Project.configure do |config|
    config.frequency = 60
    config.timeout = 30.minutes
    config.nice = 5
    config.environment_variables = {"BUILD_CLASSIC" => "1"}
    config.command = "scripts/build-realmz-mac.sh"
end

