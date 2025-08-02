package com.kryosette.kryostat;

import com.kryosette.storage.config.StorageConfig;
import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.context.annotation.ComponentScan;
import org.springframework.context.annotation.FilterType;
import org.springframework.context.annotation.Import;

@SpringBootApplication
@Import({
		StorageConfig.class,
})
@ComponentScan(excludeFilters = @ComponentScan.Filter(
		type = FilterType.ASSIGNABLE_TYPE,
		classes = {
				StorageConfig.class
		}
))
public class KryostatApplication {

	public static void main(String[] args) {
		SpringApplication.run(KryostatCommonApplication.class, args);
	}

}
